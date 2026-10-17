import argparse
import platform
import subprocess
import sys
from pathlib import Path

SDK_DIRECTORY = Path(__file__).resolve().parent
REPOSITORY_ROOT = SDK_DIRECTORY.parent
IMPORT_LIBRARY_NAME = "soh.lib"


def is_windows():
    return platform.system() == "Windows"


def find_import_library(soh_lib, game):
    candidates = [soh_lib] if soh_lib else []
    if game:
        candidates.append(game / IMPORT_LIBRARY_NAME)
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise SystemExit(
        "soh.lib not found. It ships next to soh.exe in every Windows release of Unbound "
        "(and as the soh.lib release asset). Pass --game <folder with soh.exe and soh.lib> or --soh-lib <path>.")


def run(command):
    print("+", " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], check=True)


def main():
    parser = argparse.ArgumentParser(
        description="Build every mod in mod-sdk/mods (and the templates) and install them into the game.")
    parser.add_argument("--game", type=Path, help="Game folder: built mods are copied into its mods/ folder")
    parser.add_argument("--soh-lib", type=Path, help="Windows: soh.lib, when it is not in the game folder")
    parser.add_argument("--mod", action="append", default=[], help="Build only this mod (repeatable)")
    parser.add_argument("--config", default="Release", choices=("Release", "Debug", "RelWithDebInfo"))
    parser.add_argument("--build-dir", type=Path, default=REPOSITORY_ROOT / "build" / "mod-sdk")
    parser.add_argument("--no-templates", action="store_true", help="Skip the SDK templates")
    args = parser.parse_args()

    configure = ["cmake", "-S", SDK_DIRECTORY, "-B", args.build_dir, f"-DCMAKE_BUILD_TYPE={args.config}",
                 f"-DUNBOUND_BUILD_TEMPLATES={'OFF' if args.no_templates else 'ON'}",
                 f"-DUNBOUND_GAME_DIR={args.game.resolve().as_posix() if args.game else ''}"]
    if is_windows():
        configure.append(f"-DUNBOUND_IMPORT_LIBRARY={find_import_library(args.soh_lib, args.game).as_posix()}")
    run(configure)

    build = ["cmake", "--build", args.build_dir, "--config", args.config, "--parallel"]
    for mod in args.mod:
        build += ["--target", mod]
    run(build)

    packages = sorted((args.build_dir / "packages" / args.config).glob("*.o2r"))
    print(f"\n{len(packages)} package(s) in {args.build_dir / 'packages' / args.config}")
    if args.game:
        print(f"Installed into {args.game / 'mods'}")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
