import argparse
import hashlib
import json
import re
import sys
import time
import zipfile
from pathlib import Path

import mod_signature

# The store catalog is one JSON payload, signed like a mod, that the game reads from the "mods" release.
# Each signature request only rewrites the entries of its own issue; everything else is carried over.
CATALOG_FORMAT = 1
MOD_ID = re.compile(r"^[a-z0-9_-]{1,64}$")
PACK_ID = re.compile(r"^[a-z0-9-]{1,40}$")
MAX_PACKS = 20
MAX_NAME_LENGTH = 60


def empty_catalog():
    return {"format": CATALOG_FORMAT, "sequence": 0, "mods": {}, "packs": [], "links": [], "sources": {}}


def read_catalog(path):
    if path is None or not path.is_file():
        return empty_catalog()
    return {**empty_catalog(), **json.loads(path.read_text(encoding="utf-8"))}


def find_mod_directories(mods_directory):
    return sorted(child for child in mods_directory.iterdir() if (child / "manifest.json").is_file())


def source_hashes(mods_directory):
    hashes = {}
    for mod in find_mod_directories(mods_directory):
        files = {path.relative_to(mod).as_posix(): path.read_bytes()
                 for path in sorted(mod.rglob("*")) if path.is_file()}
        hashes[mod.name] = mod_signature.content_hash(files)
    return hashes


def read_packs(mods_directory):
    path = mods_directory / "packs.json"
    if not path.is_file():
        return []
    packs = json.loads(path.read_text(encoding="utf-8")).get("packs", [])
    if not isinstance(packs, list) or len(packs) > MAX_PACKS:
        raise ValueError(f"packs.json holds at most {MAX_PACKS} packs")
    known = {mod.name for mod in find_mod_directories(mods_directory)}
    for pack in packs:
        if not isinstance(pack, dict) or not PACK_ID.match(str(pack.get("id", ""))):
            raise ValueError("every pack needs an 'id' of lowercase letters, digits and dashes")
        if not isinstance(pack.get("name"), str) or not 0 < len(pack["name"]) <= MAX_NAME_LENGTH:
            raise ValueError(f"pack '{pack['id']}' needs a 'name' of up to {MAX_NAME_LENGTH} characters")
        mods = pack.get("mods")
        if not isinstance(mods, list) or not all(isinstance(mod, str) and MOD_ID.match(mod) for mod in mods):
            raise ValueError(f"pack '{pack['id']}' needs 'mods', a list of mod folder names")
        unknown = sorted(set(mods) - known)
        if unknown:
            raise ValueError(f"pack '{pack['id']}' lists mods this folder does not have: {', '.join(unknown)}")
    return [{"id": pack["id"], "name": pack["name"], "mods": pack["mods"]} for pack in packs]


def read_package_manifest(package):
    with zipfile.ZipFile(package) as archive:
        return json.loads(archive.read("manifest.json"))


def file_hash(path):
    return hashlib.blake2b(path.read_bytes(), digest_size=64).hexdigest()


def plan(catalog, packages, hashes, issue, trusted, approved, held_permissions, major):
    """Which built mods to sign and publish, which are already published, and which wait or are refused."""
    source = catalog["sources"].get(str(issue))
    allowed = set(source["permissions"]) if source else None
    result = {"modapi": major, "publish": [], "unchanged": [], "held": {}, "rejected": {}}
    for package in sorted(packages):
        mod_id = package.stem
        manifest = read_package_manifest(package)
        existing = catalog["mods"].get(mod_id)
        build = (existing or {}).get("builds", {}).get(str(major), {})
        new_permissions = sorted(set(manifest.get("permissions", [])) - allowed) if allowed is not None else []
        if not MOD_ID.match(mod_id):
            result["rejected"][mod_id] = "the folder name must be lowercase letters, digits, '_' or '-'"
        elif existing and existing["issue"] != issue:
            result["rejected"][mod_id] = f"the name '{mod_id}' already belongs to #{existing['issue']}"
        # Packages from an SDK older than the field were built for major 1, as the game's loader assumes too.
        elif manifest.get("modapi", 1) != major:
            result["rejected"][mod_id] = f"it was built for mod API {manifest.get('modapi', 1)}, not {major}"
        elif existing and build.get("source_hash") == hashes.get(mod_id) and existing.get("listed", True):
            result["unchanged"].append(mod_id)
        elif new_permissions and not trusted and not (approved and set(new_permissions) <= held_permissions):
            result["held"][mod_id] = new_permissions
        else:
            result["publish"].append(mod_id)
    return result


def apply(catalog, plan_result, request, signed, hashes, packs, links, release_base):
    issue = request["issue"]
    major = str(plan_result["modapi"])
    source = catalog["sources"].get(str(issue), {})
    permissions = set(source.get("permissions", []))
    for mod_id in plan_result["publish"]:
        package = signed / f"{mod_id}.o2r"
        manifest = read_package_manifest(package)
        asset = f"{mod_id}-api{major}.o2r"
        existing = catalog["mods"].get(mod_id, {})
        builds = dict(existing.get("builds", {}))
        builds[major] = {"file": asset, "url": f"{release_base}/{asset}", "hash": file_hash(package),
                         "size": package.stat().st_size, "source_hash": hashes.get(mod_id, "")}
        catalog["mods"][mod_id] = {
            "name": manifest.get("name", mod_id), "author": manifest.get("author", request["author"]),
            "description": manifest.get("description", ""), "version": str(manifest.get("version", "")),
            "repository": request["repository"], "commit": request["commit"], "issue": issue,
            "owner": request["author"], "permissions": manifest.get("permissions", []),
            "dependencies": manifest.get("dependencies", []), "listed": existing.get("listed", True),
            "builds": builds,
        }
        permissions |= set(manifest.get("permissions", []))

    catalog["sources"][str(issue)] = {"repository": request["repository"], "directory": request["directory"],
                                      "author": request["author"], "commit": request["commit"],
                                      "permissions": sorted(permissions)}
    owned = {mod_id for mod_id, mod in catalog["mods"].items() if mod["issue"] == issue}
    catalog["packs"] = [pack for pack in catalog["packs"] if pack.get("issue") != issue]
    catalog["packs"] += [{"id": f"{issue}-{pack['id']}", "name": pack["name"], "author": request["author"],
                          "issue": issue, "mods": [mod for mod in pack["mods"] if mod in owned]} for pack in packs]
    catalog["links"] = links
    catalog["sequence"] = max(catalog["sequence"] + 1, int(time.time()))
    return catalog


def set_listed(catalog, issue, mod_id, listed):
    mod = catalog["mods"].get(mod_id)
    if mod is None or mod["issue"] != issue:
        raise ValueError(f"'{mod_id}' is not one of the mods of #{issue}")
    mod["listed"] = listed
    catalog["sequence"] = max(catalog["sequence"] + 1, int(time.time()))
    return catalog


def summarize(plan_result, catalog):
    lines = []
    if plan_result["publish"]:
        published = [f"[{catalog['mods'][mod]['name']} {catalog['mods'][mod]['version']}]"
                     f"({catalog['mods'][mod]['builds'][str(plan_result['modapi'])]['url']})"
                     for mod in plan_result["publish"]]
        lines.append(f"✔ Signed and in the store (mod API {plan_result['modapi']}): " + ", ".join(published))
    if plan_result["unchanged"]:
        lines.append("Unchanged, so not signed again: " + ", ".join(plan_result["unchanged"]))
    for mod, permissions in plan_result["held"].items():
        lines.append(f"⏸ {mod} now asks for {', '.join(permissions)}: waiting for a maintainer")
    for mod, reason in plan_result["rejected"].items():
        lines.append(f"✘ {mod}: {reason}")
    return "\n".join(lines) or "Nothing to publish."


def read_json(path, default):
    return json.loads(path.read_text(encoding="utf-8")) if path and path.is_file() else default


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="Build the NEI-Store catalog.")
    commands = parser.add_subparsers(dest="command", required=True)

    hashes = commands.add_parser("source-hashes", help="Hash each mod's sources, to skip unchanged mods")
    hashes.add_argument("--mods-directory", type=Path, required=True)

    packs = commands.add_parser("packs", help="Validate and print the mods folder's packs.json")
    packs.add_argument("--mods-directory", type=Path, required=True)

    plan_parser = commands.add_parser("plan", help="Decide what to publish from freshly built packages")
    plan_parser.add_argument("--catalog", type=Path)
    plan_parser.add_argument("--packages", type=Path, required=True)
    plan_parser.add_argument("--source-hashes", type=Path, required=True)
    plan_parser.add_argument("--issue", type=int, required=True)
    plan_parser.add_argument("--modapi-major", type=int, required=True)
    plan_parser.add_argument("--trusted", action="store_true")
    plan_parser.add_argument("--approved", action="store_true")
    plan_parser.add_argument("--held-permissions", default="")
    plan_parser.add_argument("--output", type=Path, required=True)

    apply_parser = commands.add_parser("apply", help="Write the next catalog payload")
    apply_parser.add_argument("--catalog", type=Path)
    apply_parser.add_argument("--plan", type=Path, required=True)
    apply_parser.add_argument("--signed", type=Path, required=True)
    apply_parser.add_argument("--source-hashes", type=Path, required=True)
    apply_parser.add_argument("--packs", type=Path)
    apply_parser.add_argument("--links", type=Path)
    apply_parser.add_argument("--issue", type=int, required=True)
    apply_parser.add_argument("--repository", required=True)
    apply_parser.add_argument("--directory", required=True)
    apply_parser.add_argument("--commit", required=True)
    apply_parser.add_argument("--author", required=True)
    apply_parser.add_argument("--release-base", required=True)
    apply_parser.add_argument("--output", type=Path, required=True)
    apply_parser.add_argument("--summary", type=Path, required=True)

    listing = commands.add_parser("listing", help="Apply an /unlist or /relist command")
    listing.add_argument("--catalog", type=Path)
    listing.add_argument("--issue", type=int, required=True)
    listing.add_argument("--command", required=True)
    listing.add_argument("--output", type=Path, required=True)
    listing.add_argument("--summary", type=Path, required=True)

    args = parser.parse_args()
    if args.command == "source-hashes":
        print(json.dumps(source_hashes(args.mods_directory)))
    elif args.command == "packs":
        print(json.dumps(read_packs(args.mods_directory)))
    elif args.command == "plan":
        held = {permission for permission in args.held_permissions.split(",") if permission}
        result = plan(read_catalog(args.catalog), list(args.packages.glob("*.o2r")),
                      read_json(args.source_hashes, {}), args.issue, args.trusted, args.approved, held,
                      args.modapi_major)
        write_json(args.output, result)
        print(json.dumps(result, indent=2))
    elif args.command == "apply":
        plan_result = read_json(args.plan, {})
        request = {"issue": args.issue, "repository": args.repository, "directory": args.directory,
                   "commit": args.commit, "author": args.author}
        catalog = apply(read_catalog(args.catalog), plan_result, request, args.signed,
                        read_json(args.source_hashes, {}), read_json(args.packs, []), read_json(args.links, []),
                        args.release_base.rstrip("/"))
        write_json(args.output, catalog)
        args.summary.write_text(summarize(plan_result, catalog) + "\n", encoding="utf-8")
    else:
        name, _, mod_id = args.command.partition(":")
        if name not in ("unlist", "relist"):
            raise ValueError(f"unknown listing command '{args.command}'")
        catalog = set_listed(read_catalog(args.catalog), args.issue, mod_id, name == "relist")
        write_json(args.output, catalog)
        verb = "back in" if name == "relist" else "out of"
        args.summary.write_text(f"{mod_id} is {verb} the store.\n", encoding="utf-8")


if __name__ == "__main__":
    try:
        main()
    except ValueError as error:
        sys.exit(f"error: {error}")
