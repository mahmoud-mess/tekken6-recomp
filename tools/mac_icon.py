#!/usr/bin/env python3
"""Make the macOS app icon from the game's own dashboard art.

The disc carries an `nxeart` package (an STFS/PIRS container) holding the Xbox 360
dashboard background, a 1920x1080 key art of Kazuya, Heihachi and Jin. This pulls
that JPEG out, crops Kazuya's head and shoulders and writes a 1024x1024 PNG on the
standard macOS icon grid (824 px rounded square, 100 px margin). No game art is
kept in the repo; the icon is made from your own copy at packaging time.

    tools/mac_icon.py game/nxeart out.png

Needs Pillow for the rounded mask; without it the square crop is written as is.
"""
import io
import struct
import sys

CROP = (700, 40, 1180, 520)  # Kazuya in the 1920x1080 background
ART = "nxebg.jpg"


def stfs_file(data: bytes, wanted: str) -> bytes:
    header_size = struct.unpack(">I", data[0x340:0x344])[0]
    vd = data[0x379:0x379 + 0x24]
    blocks_per_hash = 1 if vd[2] & 1 else 2
    table_blocks = struct.unpack("<H", vd[3:5])[0]
    table_start = int.from_bytes(vd[5:8], "little")
    base = (header_size + 0xFFF) & ~0xFFF

    def block(index: int) -> bytes:
        b = index
        for level in (170, 28900, 4913000):
            b += ((index + level) // level) * blocks_per_hash
            if index < level:
                break
        offset = base + (b << 12)
        return data[offset:offset + 0x1000]

    table = b"".join(block(table_start + i) for i in range(table_blocks))
    for i in range(0, len(table), 0x40):
        entry = table[i:i + 0x40]
        flags = entry[0x28]
        name = entry[:flags & 0x3F].decode("latin1")
        if not name or name != wanted:
            continue
        if not flags & 0x40:
            raise SystemExit(f"{wanted}: fragmented file in nxeart, not supported")
        start = int.from_bytes(entry[0x2F:0x32], "little")
        size = struct.unpack(">I", entry[0x34:0x38])[0]
        count = (size + 0xFFF) // 0x1000
        return b"".join(block(start + j) for j in range(count))[:size]
    raise SystemExit(f"{wanted} not found in nxeart")


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    jpeg = stfs_file(open(sys.argv[1], "rb").read(), ART)
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        import subprocess
        import tempfile
        with tempfile.NamedTemporaryFile(suffix=".jpg") as tmp:
            tmp.write(jpeg)
            tmp.flush()
            x0, y0, x1, y1 = CROP
            subprocess.run(["sips", "-s", "format", "png", "-c", str(y1 - y0), str(x1 - x0),
                            "--cropOffset", str(y0), str(x0), "-z", "1024", "1024",
                            tmp.name, "--out", sys.argv[2]], check=True, stdout=subprocess.DEVNULL)
        return

    body, margin, radius, ss = 824, 100, 185, 4
    art = Image.open(io.BytesIO(jpeg)).convert("RGB").crop(CROP).resize((body, body), Image.LANCZOS)
    mask = Image.new("L", (body * ss, body * ss), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, body * ss - 1, body * ss - 1), radius=radius * ss, fill=255)
    icon = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    icon.paste(art, (margin, margin), mask.resize((body, body), Image.LANCZOS))
    icon.save(sys.argv[2])


if __name__ == "__main__":
    main()
