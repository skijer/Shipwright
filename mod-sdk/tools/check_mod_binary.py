import argparse
import json
import re
import sys
import tempfile
import zipfile
from pathlib import Path

# Mirrors soh/soh/ModApi/ModTrust/ModTrust.cpp, which shows the same findings when asking about an unsigned mod.
WINDOWS_RUNTIME_PREFIXES = ("vcruntime140", "msvcp140", "ucrtbase", "api-ms-win-crt-")
WINDOWS_STARTUP_KERNEL_IMPORTS = {
    "DisableThreadLibraryCalls", "GetCurrentProcess", "GetCurrentProcessId", "GetCurrentThreadId",
    "GetSystemTimeAsFileTime", "InitializeSListHead", "IsDebuggerPresent", "IsProcessorFeaturePresent",
    "QueryPerformanceCounter", "RtlCaptureContext", "RtlLookupFunctionEntry", "RtlVirtualUnwind",
    "SetUnhandledExceptionFilter", "TerminateProcess", "UnhandledExceptionFilter",
}
WINDOWS_DANGEROUS_RUNTIME_IMPORTS = {
    "system", "_wsystem", "_popen", "_wpopen", "fopen", "fopen_s", "_wfopen", "_wfopen_s", "_fsopen", "_wfsopen",
    "freopen", "freopen_s", "_open", "_wopen", "_sopen", "_sopen_s", "_wsopen", "_wsopen_s", "_creat", "remove",
    "_wremove", "rename", "_wrename", "_unlink", "_wunlink", "_rmdir", "_wrmdir", "_mkdir", "_wmkdir",
    "_Thrd_create", "_beginthread", "_beginthreadex",
}
WINDOWS_DANGEROUS_RUNTIME_PREFIXES = ("_execl", "_execv", "_wexecl", "_wexecv", "_spawn", "_wspawn", "__std_fs_",
                                      "?_Fiopen")
POSIX_LIBRARIES = {
    "linux_x64": {"libc.so.6", "libm.so.6", "libstdc++.so.6", "libgcc_s.so.1", "ld-linux-x86-64.so.2"},
    "darwin": {"/usr/lib/libSystem.B.dylib", "/usr/lib/libc++.1.dylib", "/usr/lib/libc++abi.dylib"},
}
POSIX_DANGEROUS_IMPORTS = {
    "system", "popen", "fopen", "fopen64", "freopen", "freopen64", "open", "open64", "openat", "openat64", "creat",
    "creat64", "remove", "unlink", "unlinkat", "rename", "renameat", "rmdir", "mkdir", "mkdirat", "opendir",
    "chmod", "fchmod", "chown", "symlink", "link", "truncate", "ftruncate", "fork", "vfork", "clone", "execv",
    "execve", "execvp", "execvpe", "execl", "execlp", "execle", "fexecve", "posix_spawn", "posix_spawnp",
    "socket", "socketpair", "connect", "bind", "listen", "accept", "accept4", "send", "sendto", "sendmsg", "recv",
    "recvfrom", "recvmsg", "getaddrinfo", "gethostbyname", "dlopen", "dlmopen", "dlsym", "dlvsym", "mprotect",
    "mmap", "mmap64", "ptrace", "syscall", "pthread_create",
}
POSIX_DANGEROUS_SYMBOL_PARTS = re.compile(r"basic_ofstream|basic_ifstream|basic_fstream|basic_filebuf|filesystem|"
                                          r"St6thread")
HOST_TRUST_INTERNALS = re.compile(r"ModPermissions_|ModLoader_|ModTrust_|UserFiles_ApplyPending|crypto_|"
                                  r"kTrustedModSigningKey|kRevokedMod")
EXECUTABLE_MAGIC = (b"MZ", b"\x7fELF", b"\xfe\xed\xfa\xce", b"\xfe\xed\xfa\xcf", b"\xce\xfa\xed\xfe",
                    b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe")


def parse_binary(data):
    import lief

    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "binary"
        path.write_bytes(data)
        binary = lief.parse(str(path))
    if binary is None:
        raise ValueError("not a binary LIEF can read")
    return binary


def is_allowed_windows_import(library, function):
    if library == "soh.exe":
        return not HOST_TRUST_INTERNALS.search(function)
    if library == "kernel32.dll":
        return function in WINDOWS_STARTUP_KERNEL_IMPORTS
    if not library.startswith(WINDOWS_RUNTIME_PREFIXES):
        return False
    return function not in WINDOWS_DANGEROUS_RUNTIME_IMPORTS and not function.startswith(
        WINDOWS_DANGEROUS_RUNTIME_PREFIXES)


def check_windows_imports(binary):
    problems = []
    for library in binary.imports:
        for entry in library.entries:
            function = f"#{entry.ordinal}" if entry.is_ordinal else entry.name
            if not is_allowed_windows_import(library.name.lower(), function):
                problems.append(f"imports {library.name}!{function}")
    return problems


def check_posix_imports(binary, platform):
    libraries = [getattr(library, "name", library) for library in binary.libraries]
    problems = [f"links {library}" for library in libraries if library not in POSIX_LIBRARIES[platform]]
    for function in binary.imported_functions:
        name = function.name.split("@")[0]
        if platform == "darwin" and name.startswith("_"):
            name = name[1:]
        if (name in POSIX_DANGEROUS_IMPORTS or POSIX_DANGEROUS_SYMBOL_PARTS.search(name) or
                HOST_TRUST_INTERNALS.search(name)):
            problems.append(f"imports {function.name}")
    return problems


def check_binary(platform, data):
    binary = parse_binary(data)
    if platform.startswith("windows"):
        return check_windows_imports(binary)
    if platform in POSIX_LIBRARIES:
        return check_posix_imports(binary, platform)
    return [f"unknown platform {platform}"]


def check_package(package):
    problems = []
    with zipfile.ZipFile(package) as archive:
        manifest = json.loads(archive.read("manifest.json"))
        binaries = manifest.get("binaries", {})
        if not binaries:
            return [f"{package.name}: declares no native binary"]
        for name in archive.namelist():
            if name.startswith("bin/") and name not in binaries.values():
                problems.append(f"{package.name}: {name} is under bin/ but no platform loads it")
            elif not name.startswith("bin/") and archive.read(name)[:4].startswith(EXECUTABLE_MAGIC):
                problems.append(f"{package.name}: {name} is an executable outside bin/")
        for platform, path in sorted(binaries.items()):
            problems += [f"{package.name} ({platform}): {problem}"
                         for problem in check_binary(platform, archive.read(path))]
    return problems


def main():
    parser = argparse.ArgumentParser(
        description="Refuse mod binaries that call into the operating system instead of the game.")
    parser.add_argument("packages", type=Path, nargs="+")
    args = parser.parse_args()

    problems = []
    for package in args.packages:
        problems += check_package(package)
    for problem in problems:
        print(f"::error::{problem}")
    if problems:
        sys.exit(f"{len(problems)} problem(s): these binaries cannot be signed")
    print(f"Checked {len(args.packages)} package(s)")


if __name__ == "__main__":
    main()
