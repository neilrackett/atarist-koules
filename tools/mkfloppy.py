#!/usr/bin/env python3
# Changes for Atari ST/STE with STDL
# Copyright(c)2026 by Neil Rackett
# Build a TOS-readable FAT12 floppy image (.ST) from a list of files.
#
#   tools/mkfloppy.py -o dist/KOULES.ST [-s 720|1440] [-l LABEL] \
#       dist/KOULES.TOS=AUTO/KOULES.PRG dist/*.WAV
#
# Each argument is either a path, which lands in the image root under
# its own name, or SRC=DEST, which puts it at DEST inside the image;
# intermediate directories are created as needed.  Names have to fit
# GEMDOS 8.3, so the rename to KOULES.PRG happens here rather than in
# the build tree.
#
# Written in Python with no external tools because the build runs
# inside atarist-toolkit-docker, which has python3 but no mtools.
# The image is an ordinary DOS-formatted floppy - the boot sector is
# not executable, which is all TOS needs: GEMDOS runs \AUTO\*.PRG
# from the boot drive whether or not the disk carries a boot loader.

import argparse
import os
import struct
import sys
import time

SECTOR = 512

# The two formats a real drive writes.  spc/root/spf are the values
# every PC and every ST has agreed on since the 80s; TOS reads them
# out of the BPB, so nothing here is load bearing beyond internal
# consistency, but matching convention keeps host tools happy too.
GEOMETRY = {
    720: dict(sectors=1440, spc=2, root=112, spf=3, spt=9, heads=2,
              media=0xF9),
    1440: dict(sectors=2880, spc=1, root=224, spf=9, spt=18, heads=2,
               media=0xF0),
}


def fail(msg):
    sys.stderr.write("mkfloppy: %s\n" % msg)
    sys.exit(1)


def namefield(name):
    """Split a name into the 11-byte padded 8.3 field, or fail."""
    name = name.upper()
    stem, dot, ext = name.partition(".")
    if dot and "." in ext:
        fail("%s: more than one dot in an 8.3 name" % name)
    if not stem or len(stem) > 8 or len(ext) > 3:
        fail("%s: does not fit GEMDOS 8.3" % name)
    for c in stem + ext:
        if c in '"*+,/:;<=>?[\\]|' or ord(c) < 0x20:
            fail("%s: %r is not legal in a GEMDOS name" % (name, c))
    return ("%-8s%-3s" % (stem, ext)).encode("latin-1")


def dostime(epoch):
    t = time.localtime(epoch)
    year = max(t.tm_year, 1980) - 1980
    date = (year << 9) | (t.tm_mon << 5) | t.tm_mday
    stamp = (t.tm_hour << 11) | (t.tm_min << 5) | (t.tm_sec // 2)
    return stamp, date


class Node(object):
    """A file (src set) or a directory (children set)."""

    def __init__(self, name, src=None):
        self.name = name
        self.src = src
        self.children = {}
        self.data = b""
        self.size = 0
        self.mtime = time.time()
        self.first = 0

    @property
    def isdir(self):
        return self.src is None


def add(root, dest, src):
    """Attach src at image path dest, creating parent directories."""
    parts = [p for p in dest.replace("\\", "/").split("/") if p]
    node = root
    for part in parts[:-1]:
        child = node.children.get(part.upper())
        if child is None:
            child = Node(part)
            node.children[part.upper()] = child
        elif not child.isdir:
            fail("%s: %s is a file, not a directory" % (dest, part))
        node = child
    leaf = parts[-1]
    if leaf.upper() in node.children:
        fail("%s: already in the image" % dest)
    with open(src, "rb") as f:
        entry = Node(leaf, src=src)
        entry.data = f.read()
    entry.size = len(entry.data)
    entry.mtime = os.path.getmtime(src)
    node.children[leaf.upper()] = entry


def dirsize(node):
    """Bytes a subdirectory's own entries occupy, with . and .."""
    return 32 * (2 + len(node.children))


class Image(object):
    def __init__(self, size_kb, label):
        if size_kb not in GEOMETRY:
            fail("%d: not a floppy size (720 or 1440)" % size_kb)
        g = GEOMETRY[size_kb]
        self.__dict__.update(g)
        self.label = label
        self.nfats = 2
        self.reserved = 1
        self.rootsecs = (self.root * 32 + SECTOR - 1) // SECTOR
        self.datastart = (self.reserved + self.nfats * self.spf
                          + self.rootsecs)
        self.clustersize = self.spc * SECTOR
        self.nclusters = (self.sectors - self.datastart) // self.spc
        # cluster 0 and 1 are the two reserved FAT entries, so the
        # usable range is 2 .. nclusters + 1
        self.fat = [0] * (self.nclusters + 2)
        self.fat[0] = 0xF00 | self.media
        self.fat[1] = 0xFFF
        self.next = 2
        self.clusters = {}

    def free(self):
        return (self.nclusters - (self.next - 2)) * self.clustersize

    def alloc(self, data):
        """Store data as a cluster chain, returning the first cluster."""
        if not data:
            return 0
        n = (len(data) + self.clustersize - 1) // self.clustersize
        if self.next + n - 2 > self.nclusters:
            fail("out of space: %d bytes more than the disk holds"
                 % (len(data) - self.free()))
        first = self.next
        for i in range(n):
            c = first + i
            self.clusters[c] = data[i * self.clustersize:
                                    (i + 1) * self.clustersize]
            self.fat[c] = 0xFFF if i == n - 1 else c + 1
        self.next += n
        return first

    def entry(self, field, attr, cluster, size, mtime):
        stamp, date = dostime(mtime)
        return struct.pack("<11sB10xHHHI", field, attr, stamp, date,
                           cluster, size)

    def build(self, node, self_cluster, parent_cluster):
        """Lay out a directory depth first, returning its entries."""
        entries = b""
        if self_cluster is not None:
            entries += self.entry(b".          ", 0x10, self_cluster, 0,
                                  node.mtime)
            entries += self.entry(b"..         ", 0x10, parent_cluster, 0,
                                  node.mtime)
        for key in sorted(node.children):
            child = node.children[key]
            if child.isdir:
                # A directory's own cluster has to be known before its
                # entry can be written, and its children need it for
                # "..", so reserve the chain up front and fill it in.
                child.first = self.alloc(b"\0" * dirsize(child))
                self.write(child.first,
                           self.build(child, child.first,
                                      self_cluster or 0))
                entries += self.entry(namefield(child.name), 0x10,
                                      child.first, 0, child.mtime)
            else:
                child.first = self.alloc(child.data)
                entries += self.entry(namefield(child.name), 0x20,
                                      child.first, child.size,
                                      child.mtime)
        return entries

    def write(self, first, data):
        """Overwrite an already allocated chain with its real content."""
        c = first
        for i in range(0, len(data), self.clustersize):
            self.clusters[c] = data[i:i + self.clustersize]
            c = self.fat[c]

    def bootsector(self):
        boot = bytearray(SECTOR)
        boot[0:3] = b"\xeb\x3c\x90"          # BRA.S past the BPB
        boot[3:11] = b"KOULES  "             # OEM name
        struct.pack_into("<HBHBHHBHHHII", boot, 11, SECTOR, self.spc,
                         self.reserved, self.nfats, self.root,
                         self.sectors, self.media, self.spf, self.spt,
                         self.heads, 0, 0)
        boot[36] = 0x00                      # drive number
        boot[38] = 0x29                      # extended BPB signature
        struct.pack_into("<I", boot, 39, 0x4B4F554C)
        boot[43:54] = ("%-11s" % self.label.upper()[:11]).encode("latin-1")
        boot[54:62] = b"FAT12   "
        boot[510:512] = b"\x55\xaa"
        # A TOS boot sector executes when its 16-bit words sum to
        # 0x1234.  This one is data, so make sure it never does.
        if sum(struct.unpack(">256H", bytes(boot))) & 0xFFFF == 0x1234:
            boot[509] ^= 0x01
        return bytes(boot)

    def fattable(self):
        packed = bytearray((len(self.fat) * 3 + 1) // 2)
        for i, v in enumerate(self.fat):
            off = i * 3 // 2
            if i % 2 == 0:
                packed[off] = v & 0xFF
                packed[off + 1] = (packed[off + 1] & 0xF0) | (v >> 8)
            else:
                packed[off] = (packed[off] & 0x0F) | ((v << 4) & 0xF0)
                packed[off + 1] = (v >> 4) & 0xFF
        return bytes(packed).ljust(self.spf * SECTOR, b"\0")

    def render(self, rootdir):
        rootentries = self.build(rootdir, None, None)
        if self.label:
            field = ("%-11s" % self.label.upper()[:11]).encode("latin-1")
            rootentries = (self.entry(field, 0x08, 0, 0, rootdir.mtime)
                           + rootentries)
        if len(rootentries) > self.root * 32:
            fail("root directory holds %d entries, needs %d"
                 % (self.root, len(rootentries) // 32))
        img = bytearray(self.sectors * SECTOR)
        img[0:SECTOR] = self.bootsector()
        fat = self.fattable()
        for i in range(self.nfats):
            off = (self.reserved + i * self.spf) * SECTOR
            img[off:off + len(fat)] = fat
        off = (self.reserved + self.nfats * self.spf) * SECTOR
        img[off:off + len(rootentries)] = rootentries
        for c, data in self.clusters.items():
            off = (self.datastart + (c - 2) * self.spc) * SECTOR
            img[off:off + len(data)] = data
        return bytes(img)


def main():
    ap = argparse.ArgumentParser(
        description="build a FAT12 .ST floppy image from files")
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("-s", "--size", type=int, default=720,
                    help="720 or 1440 (KB), default 720")
    ap.add_argument("-l", "--label", default="",
                    help="volume label, 11 characters")
    ap.add_argument("files", nargs="+", metavar="SRC[=DEST]")
    args = ap.parse_args()

    root = Node("")
    root.mtime = time.time()
    for spec in args.files:
        src, sep, dest = spec.partition("=")
        add(root, dest if sep else os.path.basename(src), src)

    image = Image(args.size, args.label)
    data = image.render(root)
    with open(args.output, "wb") as f:
        f.write(data)
    print("mkfloppy: %s, %dK image, %dK free"
          % (args.output, args.size, image.free() // 1024))


if __name__ == "__main__":
    main()
