import argparse
import struct
from pathlib import Path

from PIL import Image

HEADER_SIZE = 0x40
TEXTURE_MAGIC = b"XETO"
RESOURCE_ID = 0xDEADBEEFDEADBEEF
FORMATS = {"rgba32": 1, "ia4": 7}
FORMAT_NAMES = {code: name for name, code in FORMATS.items()}


def build_header():
    header = bytearray(HEADER_SIZE)
    header[4:8] = TEXTURE_MAGIC
    struct.pack_into("<Q", header, 0x0C, RESOURCE_ID)
    return bytes(header)


def pack_rgba32(image):
    return image.convert("RGBA").tobytes()


def pack_ia4(image):
    if image.width % 2 != 0:
        raise ValueError("IA4 textures need an even width")
    luminance_alpha = image.convert("LA").tobytes()
    nibbles = [
        ((luminance_alpha[index] >> 5) << 1) | (1 if luminance_alpha[index + 1] >= 128 else 0)
        for index in range(0, len(luminance_alpha), 2)
    ]
    return bytes((nibbles[index] << 4) | nibbles[index + 1] for index in range(0, len(nibbles), 2))


def unpack_rgba32(data, width, height):
    return Image.frombytes("RGBA", (width, height), data)


def unpack_ia4(data, width, height):
    luminance_alpha = bytearray()
    for byte in data:
        for nibble in (byte >> 4, byte & 0xF):
            luminance_alpha.append((nibble >> 1) * 255 // 7)
            luminance_alpha.append(255 if nibble & 1 else 0)
    return Image.frombytes("LA", (width, height), bytes(luminance_alpha[: width * height * 2]))


PACKERS = {"rgba32": pack_rgba32, "ia4": pack_ia4}
UNPACKERS = {"rgba32": unpack_rgba32, "ia4": unpack_ia4}


def encode(png_path, texture_format):
    image = Image.open(png_path)
    pixels = PACKERS[texture_format](image)
    body = struct.pack("<IIII", FORMATS[texture_format], image.width, image.height, len(pixels))
    return build_header() + body + pixels


def decode(otex_bytes):
    if otex_bytes[4:8] != TEXTURE_MAGIC:
        raise ValueError("Not an OTEX texture resource")
    code, width, height, size = struct.unpack_from("<IIII", otex_bytes, HEADER_SIZE)
    if code not in FORMAT_NAMES:
        raise ValueError(f"Texture format {code} is not supported by this tool")
    pixels = otex_bytes[HEADER_SIZE + 16 : HEADER_SIZE + 16 + size]
    return UNPACKERS[FORMAT_NAMES[code]](pixels, width, height)


def main():
    parser = argparse.ArgumentParser(description="Convert PNG images to Unbound OTEX texture resources and back.")
    commands = parser.add_subparsers(dest="command", required=True)
    to_otex = commands.add_parser("encode", help="PNG -> OTEX")
    to_otex.add_argument("png", type=Path)
    to_otex.add_argument("output", type=Path)
    to_otex.add_argument("--format", choices=sorted(FORMATS), default="rgba32")
    to_png = commands.add_parser("decode", help="OTEX -> PNG")
    to_png.add_argument("otex", type=Path)
    to_png.add_argument("output", type=Path)
    args = parser.parse_args()

    if args.command == "encode":
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(encode(args.png, args.format))
    else:
        decode(args.otex.read_bytes()).save(args.output)
    print(args.output)


if __name__ == "__main__":
    main()
