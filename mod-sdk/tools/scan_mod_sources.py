import argparse
import json
import re
import shlex
import subprocess
import sys
from pathlib import Path

SOURCE_SUFFIXES = {".c", ".cpp", ".h", ".hpp", ".inc"}
DOCUMENT_SUFFIXES = {".md", ".txt"}
EXECUTABLE_SUFFIXES = {".dll", ".exe", ".so", ".dylib", ".bat", ".cmd", ".ps1", ".sh", ".py"}
EXECUTABLE_MAGIC = (b"MZ", b"\x7fELF", b"\xfe\xed\xfa\xce", b"\xfe\xed\xfa\xcf", b"\xce\xfa\xed\xfe",
                    b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe")
BUILD_SCRIPT_NAMES = {"cmakelists.txt", "makefile", "build.ninja"}
ALLOWED_PRAGMAS = re.compile(r"^(once|pack\b|warning\b|region\b|endregion\b|(GCC|clang) diagnostic\b)")
FORBIDDEN_HEADERS = re.compile(
    r"^(windows|winsock2?|ws2\w*|wininet|winhttp|winreg|winternl|shellapi|shlobj\w*|processthreadsapi|"
    r"libloaderapi|memoryapi|tlhelp32|psapi|dbghelp|intrin|process|io|direct|unistd|fcntl|dlfcn|spawn|"
    r"netdb|pthread|thread|future|filesystem|fstream|curl/.*|sys/.*|netinet/.*|arpa/.*|.*portable-file-dialogs)"
    r"(\.h|\.hpp)?$")
ASSEMBLY_KEYWORDS = {"asm", "__asm", "__asm__", "_asm"}
FORBIDDEN_IDENTIFIERS = ASSEMBLY_KEYWORDS | {
    "__readgsqword", "__readgsdword", "__readfsqword", "__readfsdword", "__getReg", "NtCurrentTeb", "NtCurrentPeb",
    "LoadLibraryA", "LoadLibraryW", "LoadLibraryExA", "LoadLibraryExW", "GetProcAddress", "GetModuleHandleA",
    "GetModuleHandleW", "dlopen", "dlsym", "dlmopen", "VirtualProtect", "VirtualAlloc", "mprotect", "syscall",
    "system", "_wsystem", "popen", "_popen", "_wpopen", "fopen", "fopen_s", "_wfopen", "_wfopen_s", "freopen",
    "_fsopen", "_wfsopen", "remove", "_wremove", "rename", "_wrename", "unlink", "_unlink", "rmdir", "_rmdir",
    "mkdir", "_mkdir", "fork", "vfork", "execv", "execve", "execvp", "execl", "execlp", "execle", "posix_spawn",
    "CreateProcessA", "CreateProcessW", "ShellExecuteA", "ShellExecuteW", "WinExec", "CreateThread",
    "_beginthread", "_beginthreadex", "pthread_create", "socket", "connect", "getaddrinfo", "gethostbyname",
}
KNOWN_PERMISSIONS = {"microphone", "files"}
SERVICE_PERMISSIONS = {
    "OpenMicrophone": "microphone", "ReadMicrophone": "microphone", "CloseMicrophone": "microphone",
    "Microphone_Open": "microphone", "Microphone_Read": "microphone", "Microphone_Close": "microphone",
    "PickUserFile": "files", "SaveToModsFolder": "files", "ExtractRomToModsFolder": "files",
    "UserFiles_Pick": "files", "UserFiles_SaveToMods": "files", "UserFiles_ExtractRomToMods": "files",
    "ExportArchiveFiles": "files", "ExtractRom": "files", "O2rExtractor_ExportFiles": "files",
    "O2rExtractor_Run": "files",
}
INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.MULTILINE)
PRAGMA = re.compile(r"^\s*#\s*pragma\s+(.*)$", re.MULTILINE)
LINE_MARKER = re.compile(r'^#\s*\d+\s+"((?:\\.|[^"\\])*)"')
LITERAL = re.compile(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'')
IDENTIFIER = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*\b")


def find_mod_directories(mods_directory):
    return sorted(child for child in mods_directory.iterdir() if (child / "manifest.json").is_file())


def check_layout(mods_directory):
    problems = []
    for path in sorted(mods_directory.rglob("*")):
        relative = path.relative_to(mods_directory)
        if path.is_symlink():
            problems.append(f"{relative}: symbolic links are not allowed")
            continue
        if not path.is_file():
            continue
        name = path.name.lower()
        suffix = path.suffix.lower()
        in_assets = len(relative.parts) > 2 and relative.parts[1] == "assets"
        is_known_file = name == "manifest.json" or suffix in SOURCE_SUFFIXES | DOCUMENT_SUFFIXES
        if name in BUILD_SCRIPT_NAMES or suffix == ".cmake":
            problems.append(f"{relative}: build scripts are not allowed; the signing build uses its own")
        elif suffix in EXECUTABLE_SUFFIXES or starts_like_executable(path):
            problems.append(f"{relative}: executables and scripts are not allowed")
        elif not in_assets and not is_known_file:
            problems.append(f"{relative}: unexpected file outside assets/")
    return problems


def starts_like_executable(path):
    with path.open("rb") as file:
        return file.read(4).startswith(EXECUTABLE_MAGIC)


def is_include_inside(source, header, mod_directory):
    if header.startswith(("/", "\\")) or re.match(r"^[A-Za-z]:", header):
        return False
    if ".." not in re.split(r"[/\\]", header):
        return True
    return (source.parent / header).resolve().is_relative_to(mod_directory.resolve())


def strip_comments(text):
    return re.sub(r"/\*.*?\*/|//[^\n]*", " ", text, flags=re.DOTALL)


def check_directives(mods_directory):
    problems = []
    for path in sorted(mods_directory.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        relative = path.relative_to(mods_directory)
        mod_directory = mods_directory / relative.parts[0]
        text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
        for delimiter, header in INCLUDE.findall(text):
            closer = ">" if delimiter == "<" else '"'
            spelled = f"{delimiter}{header}{closer}"
            if not is_include_inside(path, header, mod_directory):
                problems.append(f"{relative}: #include {spelled} leaves the mod's own folder")
            elif FORBIDDEN_HEADERS.match(header.lower()):
                problems.append(f"{relative}: #include {spelled} reaches the operating system")
        for pragma in PRAGMA.findall(text):
            if not ALLOWED_PRAGMAS.match(pragma.strip()):
                problems.append(f"{relative}: #pragma {pragma.strip()} is not allowed")
        if re.search(r"^\s*#\s*(import|include_next)\b", text, re.MULTILINE):
            problems.append(f"{relative}: #import and #include_next are not allowed")
    return problems


def preprocess_command(entry):
    arguments = entry.get("arguments") or shlex.split(entry["command"])
    result = []
    skip_next = False
    for argument in arguments:
        if skip_next:
            skip_next = False
            continue
        if argument == "-o":
            skip_next = True
            continue
        if argument == "-c" or argument.startswith("-o"):
            continue
        result.append(argument)
    return result + ["-E"]


def read_declared_permissions(mod_directory):
    permissions = json.loads((mod_directory / "manifest.json").read_text(encoding="utf-8")).get("permissions", [])
    if not isinstance(permissions, list) or not all(isinstance(permission, str) for permission in permissions):
        return None
    return set(permissions)


def check_manifests(mods_directory):
    problems = []
    for mod in find_mod_directories(mods_directory):
        declared = read_declared_permissions(mod)
        if declared is None:
            problems.append(f"{mod.name}/manifest.json: 'permissions' must be a list of names")
            continue
        for unknown in sorted(declared - KNOWN_PERMISSIONS):
            problems.append(f"{mod.name}/manifest.json: '{unknown}' is not a permission the game knows")
    return problems


def is_forbidden_use(code, match):
    """Inline assembly anywhere; any other name only when called as a free function, so a variable or member
    that shares the name passes. A call reached some other way still shows up in the binary's imports."""
    if match.group(0) in ASSEMBLY_KEYWORDS:
        return True
    if code[:match.start()].rstrip().endswith((".", "->", "std::")):
        return False
    return code[match.end():].lstrip().startswith("(")


def scan_tokens(preprocessed, submitted_root, mods_directory):
    """Forbidden identifiers, and which permissions each mod's services need, in the code the mod itself wrote."""
    problems = set()
    used_permissions = {}
    current_file = None
    for line in preprocessed.splitlines():
        marker = LINE_MARKER.match(line)
        if marker:
            current_file = Path(marker.group(1).replace("\\\\", "\\")).resolve()
            continue
        if current_file is None or not current_file.is_relative_to(submitted_root):
            continue
        code = LITERAL.sub(" ", line)
        for match in IDENTIFIER.finditer(code):
            identifier = match.group(0)
            if identifier in FORBIDDEN_IDENTIFIERS and is_forbidden_use(code, match):
                problems.add(f"{current_file.relative_to(submitted_root)}: uses '{identifier}'")
            permission = SERVICE_PERMISSIONS.get(identifier)
            if permission and current_file.is_relative_to(mods_directory):
                mod = current_file.relative_to(mods_directory).parts[0]
                used_permissions.setdefault(mod, set()).add(permission)
    return problems, used_permissions


def check_preprocessed(compile_commands, mods_directory, submitted_root):
    problems = set()
    used_permissions = {}
    entries = json.loads(compile_commands.read_text(encoding="utf-8"))
    scanned = 0
    for entry in entries:
        source = Path(entry["directory"], entry["file"]).resolve()
        if not source.is_relative_to(mods_directory):
            continue
        output = subprocess.run(preprocess_command(entry), cwd=entry["directory"], capture_output=True, text=True,
                                errors="replace", check=False)
        if output.returncode != 0:
            problems.add(f"{source.relative_to(submitted_root)}: does not preprocess:\n{output.stderr[-2000:]}")
            continue
        file_problems, file_permissions = scan_tokens(output.stdout, submitted_root, mods_directory)
        problems |= file_problems
        for mod, permissions in file_permissions.items():
            used_permissions.setdefault(mod, set()).update(permissions)
        scanned += 1
    if scanned == 0:
        problems.add("No mod source was found in the compile commands")
    for mod, permissions in used_permissions.items():
        for permission in sorted(permissions - (read_declared_permissions(mods_directory / mod) or set())):
            problems.add(f"{mod}: uses a '{permission}' service but its manifest does not declare that permission")
    return problems


def main():
    parser = argparse.ArgumentParser(
        description="Refuse mod sources that reach the operating system instead of the ModApi.")
    parser.add_argument("--mods-directory", type=Path, required=True)
    parser.add_argument("--submitted-root", type=Path, required=True, help="Checkout of the submitted repository")
    parser.add_argument("--compile-commands", type=Path, help="compile_commands.json of the signing build")
    args = parser.parse_args()

    mods_directory = args.mods_directory.resolve()
    submitted_root = args.submitted_root.resolve()
    if not mods_directory.is_relative_to(submitted_root) or not mods_directory.is_dir():
        sys.exit(f"error: {args.mods_directory} is not a folder of the submitted repository")
    mods = find_mod_directories(mods_directory)
    if not mods:
        sys.exit(f"error: {args.mods_directory} holds no mod folder with a manifest.json")

    problems = check_layout(mods_directory) + check_directives(mods_directory) + check_manifests(mods_directory)
    if args.compile_commands and not problems:
        problems += sorted(check_preprocessed(args.compile_commands, mods_directory, submitted_root))
    for problem in problems:
        print(f"::error::{problem}")
    if problems:
        sys.exit(f"{len(problems)} problem(s): these sources cannot be signed")
    print(f"Scanned {len(mods)} mod(s): {', '.join(mod.name for mod in mods)}")


if __name__ == "__main__":
    main()
