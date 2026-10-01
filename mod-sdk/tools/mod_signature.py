import argparse
import hashlib
import json
import os
import re
import sys
import zipfile
from pathlib import Path

SIGNATURE_FILE = "unbound-signature.json"
SIGNATURE_DOMAIN = "unbound-mod-signature-v1"
STATEMENT_FIELDS = ("repository", "commit", "author", "workflow_run")
FIELD_PATTERNS = {
    "repository": re.compile(r"^[A-Za-z0-9-]{1,39}/[A-Za-z0-9._-]{1,100}$"),
    "commit": re.compile(r"^[0-9a-f]{40}$"),
    "author": re.compile(r"^[A-Za-z0-9-]{1,39}$"),
    "workflow_run": re.compile(r"^https://github\.com/[A-Za-z0-9._/-]+/actions/runs/[0-9]+(/attempts/[0-9]+)?$"),
}
KEY_ENVIRONMENT_VARIABLE = "UNBOUND_MOD_SIGNING_KEY"
KEY_HEADER = Path(__file__).resolve().parents[2] / "soh" / "soh" / "ModApi" / "ModTrust" / "TrustedModKey.h"
KEY_HEADER_TEMPLATE = """#pragma once

#include <cstdint>

// Written by `mod-sdk/tools/mod_signature.py generate-key`. All zeros means no key: nothing verifies.
inline constexpr uint8_t kTrustedModSigningKey[32] = {{
{rows}
}};
"""


def read_package_files(package):
    with zipfile.ZipFile(package) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError(f"{package} repeats a file name")
        files = {}
        for name in names:
            if name.endswith("/") or name == SIGNATURE_FILE:
                continue
            if not name.isascii():
                raise ValueError(f"{package}: '{name}' is not an ASCII path")
            # The game indexes 'x.meta' as 'x', so a package holding one could never verify.
            if name.endswith(".meta"):
                raise ValueError(f"{package}: '{name}' is a .meta file, which signed packages cannot hold")
            files[name] = archive.read(name)
    return files


def content_hash(files):
    digest = hashlib.blake2b(digest_size=64)
    for name in sorted(files, key=lambda path: path.encode("ascii")):
        encoded = name.encode("ascii")
        digest.update(len(encoded).to_bytes(4, "little"))
        digest.update(encoded)
        digest.update(len(files[name]).to_bytes(8, "little"))
        digest.update(files[name])
    return digest.hexdigest()


def signed_message(statement):
    lines = [SIGNATURE_DOMAIN, *(statement[field] for field in STATEMENT_FIELDS), statement["content_hash"]]
    return "\n".join(lines).encode("utf-8")


def validate_fields(fields):
    for field, pattern in FIELD_PATTERNS.items():
        if not pattern.match(fields.get(field, "")):
            raise ValueError(f"'{field}' is missing or malformed: {fields.get(field)!r}")


def load_private_key():
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

    seed = os.environ.get(KEY_ENVIRONMENT_VARIABLE, "").strip()
    if len(seed) != 64:
        raise SystemExit(f"{KEY_ENVIRONMENT_VARIABLE} must hold the 32-byte private key as 64 hex digits")
    return Ed25519PrivateKey.from_private_bytes(bytes.fromhex(seed))


def read_trusted_key(header=KEY_HEADER):
    digits = re.findall(r"0x([0-9a-fA-F]{2})", header.read_text(encoding="utf-8"))
    if len(digits) != 32:
        raise ValueError(f"{header} does not hold a 32-byte key")
    return bytes(int(digit, 16) for digit in digits)


def write_package(source, output, extra_name, extra_data):
    with zipfile.ZipFile(source) as archive:
        entries = [(name, archive.read(name)) for name in archive.namelist() if name != SIGNATURE_FILE]
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".partial")
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in entries:
            archive.writestr(name, data)
        archive.writestr(extra_name, extra_data)
    temporary.replace(output)


def sign_package(package, output, fields, private_key):
    validate_fields(fields)
    statement = {field: fields[field] for field in STATEMENT_FIELDS}
    statement["content_hash"] = content_hash(read_package_files(package))
    statement["signature"] = private_key.sign(signed_message(statement)).hex()
    write_package(package, output, SIGNATURE_FILE, json.dumps(statement, indent=2))
    return statement


def verify_package(package, public_key_bytes):
    from cryptography.exceptions import InvalidSignature
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey

    if not any(public_key_bytes):
        raise ValueError("The trusted key is all zeros: run generate-key first")
    with zipfile.ZipFile(package) as archive:
        if SIGNATURE_FILE not in archive.namelist():
            raise ValueError(f"{package} is not signed")
        statement = json.loads(archive.read(SIGNATURE_FILE))
    validate_fields(statement)
    if statement.get("content_hash") != content_hash(read_package_files(package)):
        raise ValueError(f"{package}: files changed after signing")
    try:
        Ed25519PublicKey.from_public_bytes(public_key_bytes).verify(bytes.fromhex(statement["signature"]),
                                                                    signed_message(statement))
    except (InvalidSignature, ValueError) as error:
        raise ValueError(f"{package}: the signature does not match the trusted key") from error
    return statement


def generate_key(private_key_output, header):
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

    if private_key_output.exists():
        raise SystemExit(f"{private_key_output} already exists; refusing to overwrite a signing key")
    key = Ed25519PrivateKey.generate()
    seed = key.private_bytes(serialization.Encoding.Raw, serialization.PrivateFormat.Raw,
                             serialization.NoEncryption())
    public = key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    descriptor = os.open(private_key_output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, "w", encoding="ascii") as output:
        output.write(seed.hex() + "\n")
    rows = "\n".join("    " + " ".join(f"0x{byte:02x}," for byte in public[row:row + 16]) for row in (0, 16))
    header.write_text(KEY_HEADER_TEMPLATE.format(rows=rows), encoding="utf-8", newline="\n")
    return public


def main():
    parser = argparse.ArgumentParser(description="Sign and verify Unbound mod packages.")
    commands = parser.add_subparsers(dest="command", required=True)

    sign = commands.add_parser("sign", help=f"Sign a package with the key in ${KEY_ENVIRONMENT_VARIABLE}")
    sign.add_argument("package", type=Path)
    sign.add_argument("--output", type=Path, required=True)
    for field in STATEMENT_FIELDS:
        sign.add_argument(f"--{field.replace('_', '-')}", dest=field, required=True)

    verify = commands.add_parser("verify", help="Check a package against the key compiled into the game")
    verify.add_argument("packages", type=Path, nargs="+")
    verify.add_argument("--key-header", type=Path, default=KEY_HEADER)

    generate = commands.add_parser("generate-key", help="Create the signing key and write its public half")
    generate.add_argument("--private-key-output", type=Path, required=True)
    generate.add_argument("--key-header", type=Path, default=KEY_HEADER)

    args = parser.parse_args()
    if args.command == "sign":
        fields = {field: getattr(args, field) for field in STATEMENT_FIELDS}
        statement = sign_package(args.package, args.output, fields, load_private_key())
        print(f"{args.output}: signed for {statement['repository']}@{statement['commit'][:7]}")
    elif args.command == "verify":
        public_key = read_trusted_key(args.key_header)
        for package in args.packages:
            statement = verify_package(package, public_key)
            print(f"{package}: {statement['repository']}@{statement['commit'][:7]} by {statement['author']}")
    else:
        public = generate_key(args.private_key_output, args.key_header)
        print(f"Public key {public.hex()} written to {args.key_header}")
        print(f"Private key written to {args.private_key_output}: store it as the {KEY_ENVIRONMENT_VARIABLE} "
              "secret of the mod-signing environment, then delete the file")


if __name__ == "__main__":
    try:
        main()
    except ValueError as error:
        sys.exit(f"error: {error}")
