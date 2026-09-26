"""Extract unmodified C/C++ production functions for isolated engine fixtures."""
import re


def functions(source, names=None):
    pattern = r"^(?:static[ \t]+)?[A-Za-z_][\w *]*[ \t]+(\w+)\s*\([^;{}]*\)\s*\{"
    found = {}
    for match in re.finditer(pattern, source, re.M):
        if names is not None and match[1] not in names:
            continue
        start = match.start()
        pos = match.end()
        depth = 1
        while depth:
            depth += (source[pos] == "{") - (source[pos] == "}")
            pos += 1
        found[match[1]] = source[start:pos]
    return found

def block_from(source, start):
    pos = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return source[start:pos]

def function(source, name):
    match = re.search(r'^[^\n;{}]*\b' + re.escape(name) + r'\([^;{}]*\)\s*\{', source, re.M)
    if match is None:
        raise RuntimeError(f'Missing production function: {name}')
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'
