#!/usr/bin/env python3
"""Extract a Sonic Riders (USA) GameCube disc into the folder Turbulence runs.

Usage:
    python3 extract-sonic-riders.py "Sonic Riders (USA).iso" game

Accepts a plain ISO/GCM or a CISO image. RVZ, WIA, GCZ, WBFS and NKit images
are compressed or trimmed: convert them to ISO in Dolphin first (right-click
the game > Convert File... > ISO), or use Dolphin's
Properties > Filesystem > right-click the disc > Extract Entire Disc.

The output has the same layout as Dolphin's "Extract Entire Disc":
    game/sys/{boot.bin,bi2.bin,apploader.img,fst.bin,main.dol}
    game/files/...
Only Python 3 is needed; nothing is downloaded or sent anywhere.
"""
import argparse
import hashlib
import os
import struct
import sys

DISC_ID = b"GXEE8P"
DOL_SHA256 = "8b36ae0c3c86bc249b1bf0b05ea636478589aad25a5632f21877f60c900a94a1"
REL_SHA256 = "7d850de948d27555c6ad5efb469b2856168fc8ed6806aed1af4bb4ca6a069479"
GC_MAGIC = 0xC2339F3D
WINDOWS_DEVICES = {"CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$"}
WINDOWS_DEVICES.update(f"{device}{digit}" for device in ("COM", "LPT")
                       for digit in "0123456789¹²³")


class PlainDisc:
    def __init__(self, f):
        self.f = f

    def read(self, offset, size):
        self.f.seek(offset)
        data = self.f.read(size)
        if len(data) != size:
            raise SystemExit("error: the image is truncated (read past its end)")
        return data


class CisoDisc:
    """CISO: 0x8000-byte header ("CISO", u32 LE block size, block-present map),
    then the present blocks in order. Missing blocks read as zeros."""

    HEADER = 0x8000

    def __init__(self, f):
        self.f = f
        header = f.read(self.HEADER)
        if len(header) != self.HEADER:
            raise SystemExit("error: truncated CISO header")
        self.block_size = struct.unpack_from("<I", header, 4)[0]
        if not 0x440 <= self.block_size <= 32 * 1024 * 1024:
            raise SystemExit("error: bad CISO header")
        self.index = {}
        stored = 0
        for block, present in enumerate(header[8:]):
            if present not in (0, 1):
                raise SystemExit("error: invalid CISO block table")
            if present:
                self.index[block] = stored
                stored += 1

    def read(self, offset, size):
        out = bytearray()
        while size:
            block, within = divmod(offset, self.block_size)
            take = min(size, self.block_size - within)
            if block in self.index:
                self.f.seek(self.HEADER + self.index[block] * self.block_size + within)
                chunk = self.f.read(take)
                if len(chunk) != take:
                    raise SystemExit("error: the CISO image is truncated")
                out += chunk
            else:
                out += bytes(take)
            offset += take
            size -= take
        return bytes(out)


def be32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def dol_size(header):
    end = 0x100
    for i in range(18):  # 7 text + 11 data sections
        offset, size = be32(header, i * 4), be32(header, 0x90 + i * 4)
        if size:
            end = max(end, offset + size)
    return end


def safe_name(name):
    if not name or name in (".", "..") or "/" in name or "\\" in name or "\0" in name:
        raise SystemExit(f"error: refusing unsafe file name in the disc: {name!r}")
    if os.name == "nt" and (name.endswith((".", " ")) or
                            any(ord(c) < 32 or c in '<>:"|?*' for c in name) or
                            name.split(".", 1)[0].rstrip().upper() in WINDOWS_DEVICES):
        raise SystemExit(f"error: refusing unsafe Windows file name: {name!r}")
    return name


def write(path, data):
    with open(path, "wb") as f:
        f.write(data)


def copy_file(disc, offset, size, path):
    with open(path, "xb") as out:
        while size:
            take = min(size, 8 << 20)
            out.write(disc.read(offset, take))
            offset += take
            size -= take


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("image", help="Sonic Riders (USA) .iso / .gcm / .ciso")
    ap.add_argument("output", nargs="?", default="game", help="output folder (default: game)")
    ap.add_argument("--force", action="store_true", help="extract even if the revision doesn't match")
    args = ap.parse_args()

    with open(args.image, "rb") as f:
        magic = f.read(4)
        f.seek(0)
        if magic == b"RVZ\x01" or magic == b"WIA\x01":
            raise SystemExit("error: RVZ/WIA image. Convert it to ISO in Dolphin first (see --help).")
        if magic == b"\x01\xc0\x0b\xb1":
            raise SystemExit("error: GCZ image. Convert it to ISO in Dolphin first (see --help).")
        disc = CisoDisc(f) if magic == b"CISO" else PlainDisc(f)

        boot = disc.read(0, 0x440)
        if be32(boot, 0x1C) != GC_MAGIC:
            raise SystemExit("error: not a GameCube disc image")
        if boot[:6] != DISC_ID:
            raise SystemExit(f"error: this is {boot[:6].decode(errors='replace')}, "
                             f"not Sonic Riders USA ({DISC_ID.decode()})")
        if boot[7] != 0 and not args.force:
            raise SystemExit(f"error: disc revision {boot[7]}; Turbulence needs revision 0")

        bi2 = disc.read(0x440, 0x2000)
        app_header = disc.read(0x2440, 0x20)
        app_size = 0x20 + be32(app_header, 0x14) + be32(app_header, 0x18)
        if app_size > 16 << 20:
            raise SystemExit("error: invalid apploader size")
        apploader = disc.read(0x2440, app_size)
        dol_offset, fst_offset, fst_len = be32(boot, 0x420), be32(boot, 0x424), be32(boot, 0x428)
        main_size = dol_size(disc.read(dol_offset, 0x100))
        if main_size > 32 << 20 or not 12 <= fst_len <= 16 << 20:
            raise SystemExit("error: invalid DOL or filesystem size")
        dol = disc.read(dol_offset, main_size)
        fst = disc.read(fst_offset, fst_len)

        dol_hash = hashlib.sha256(dol).hexdigest()
        if dol_hash != DOL_SHA256 and not args.force:
            raise SystemExit("error: main.dol doesn't match the version Turbulence was built for "
                             f"(sha256 {dol_hash}). Is the image modified or an NKit/scrubbed dump?")

        out = args.output
        sys_dir, files_dir = os.path.join(out, "sys"), os.path.join(out, "files")
        if os.path.exists(os.path.join(sys_dir, "main.dol")):
            raise SystemExit(f"error: {out} already contains an extracted game; pick an empty folder")
        os.makedirs(sys_dir, exist_ok=True)
        os.makedirs(files_dir, exist_ok=True)
        for name, data in (("boot.bin", boot), ("bi2.bin", bi2), ("apploader.img", apploader),
                           ("fst.bin", fst), ("main.dol", dol)):
            write(os.path.join(sys_dir, name), data)

        count = be32(fst, 8)
        names = count * 12
        if names > len(fst):
            raise SystemExit("error: corrupt file system table")

        def name_at(i):
            start = names + (be32(fst, i * 12) & 0xFFFFFF)
            return fst[start:fst.index(b"\0", start)].decode("shift_jis")

        stack = [(count, files_dir)]  # (index where this directory ends, path)
        total = 0
        for i in range(1, count):
            while i >= stack[-1][0]:
                stack.pop()
            entry = fst[i * 12:i * 12 + 12]
            path = os.path.join(stack[-1][1], safe_name(name_at(i)))
            if entry[0] & 1:  # directory: field 2 is the index after its last child
                end = be32(entry, 8)
                if not i < end <= stack[-1][0]:
                    raise SystemExit("error: corrupt directory range")
                os.mkdir(path)
                stack.append((end, path))
            else:
                size = be32(entry, 8)
                copy_file(disc, be32(entry, 4), size, path)
                total += size
            print(f"\rextracting {i}/{count - 1} files", end="", file=sys.stderr)
        print(file=sys.stderr)

    rel = os.path.join(files_dir, "_Main.rel")
    if not os.path.isfile(rel):
        raise SystemExit("error: files/_Main.rel is missing from the disc")
    with open(rel, "rb") as f:
        rel_hash = hashlib.sha256(f.read()).hexdigest()
    if rel_hash != REL_SHA256 and not args.force:
        raise SystemExit(f"error: files/_Main.rel doesn't match (sha256 {rel_hash})")
    print(f"done: {count - 1} files, {total / 1e6:.0f} MB in {os.path.abspath(out)}")


if __name__ == "__main__":
    main()
