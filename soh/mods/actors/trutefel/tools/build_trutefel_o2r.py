#!/usr/bin/env python3
"""
build_trutefel_o2r.py — convert Fast64/HackerOoT-style custom enemy C exports into
a libultraship mod archive (trutefel-enemies.o2r) + generated compiled-asset C files.

Input : extracted mod folders (object_*.c with u64 textures, Vtx, Gfx, StandardLimb,
        FlexSkeletonHeader; g*Anim.c animation data)
Output: - trutefel-enemies.o2r  (zip: OTEX textures, OARR vtx, ODLT display lists)
        - <object>_assets.inc.c / .h  per enemy (skeleton limbs -> OTR path DL refs,
          animations verbatim, eye-texture path arrays) — identical file compiles in
          both soh (OoT) and 2ship (MM).

GBI encoding is done by expanding the REAL macros from mm/include/PR/gbi.h (F3DEX2)
with a mini C preprocessor, so command words match compiler output exactly.
OTR reference opcodes (0x20 SETTIMG_HASH / 0x31 DL_HASH / 0x32 VTX_HASH) + CRC64
follow OTRExporter/DisplayListExporter.cpp.
"""
import os, re, sys, struct, zipfile, math, json

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), *[os.pardir] * 5))
GBI_H = os.path.join(REPO, "libultraship", "include", "libultraship", "libultra", "gbi.h")
SCRATCH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENEMIES_DIR = os.path.join(SCRATCH, "enemies")
OUT_DIR = os.path.join(SCRATCH, "out")
O2R_PATH = os.path.join(OUT_DIR, "trutefel-enemies.o2r")

OTR_PREFIX = "objects/trutefel"   # resource path root inside the archive

ENEMIES = [
    # (folder, object dir name, skel file base, object name)
    (os.path.join(ENEMIES_DIR, "Miniblin", "Miniblin"), "object_miniblin", "gMiniblinSkel"),
    (os.path.join(ENEMIES_DIR, "Molmauk", "Molmauk"), "object_hammergeist", "gHammergeistSkel"),
    (os.path.join(ENEMIES_DIR, "ScissorsBeetle (1)", "ScissorsBeetle"), "object_sbeetle", "gScissorsBeetleSkel"),
]

# ---------------------------------------------------------------------------
# CRC64 (ECMA-182 poly, init all-ones, MSB-first, no final xor) — matches
# libultraship StrHash64. Sanity pair asserted below.
# ---------------------------------------------------------------------------
_POLY = 0x42F0E1EBA9EA3693
_crc_table = []
def _mk_table():
    for i in range(256):
        crc = i << 56
        for _ in range(8):
            if crc & (1 << 63):
                crc = ((crc << 1) ^ _POLY) & 0xFFFFFFFFFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFFFFFFFFFF
        _crc_table.append(crc)
_mk_table()

def crc64(s: str) -> int:
    crc = 0xFFFFFFFFFFFFFFFF
    for b in s.encode("utf-8"):
        crc = ((crc << 8) & 0xFFFFFFFFFFFFFFFF) ^ _crc_table[((crc >> 56) ^ b) & 0xFF]
    return crc

assert crc64("custom/prelude/testroom_scene/batch0Vtx") == 0x6F9B12B6E87B2B65, \
    "CRC64 implementation mismatch!"

# ---------------------------------------------------------------------------
# Mini C preprocessor over gbi.h (enough for object-like + function-like macros,
# ##-pasting, #ifdef/#if defined()/#else/#elif/#endif, #undef).
# ---------------------------------------------------------------------------
PREDEFINED = {"F3DEX_GBI_2", "_LANGUAGE_C"}

class MacroDef:
    __slots__ = ("params", "body")
    def __init__(self, params, body):
        self.params = params      # None for object-like, list for function-like
        self.body = body

def _strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text

def load_gbi_macros(path):
    text = _strip_comments(open(path, encoding="utf-8", errors="replace").read())
    # join line continuations
    text = text.replace("\\\n", " ")
    macros = {}
    cond_stack = []  # each entry: (currently_active, ever_active)
    def active():
        return all(c[0] for c in cond_stack)
    def eval_cond(expr):
        e = expr
        e = re.sub(r"defined\s*\(\s*(\w+)\s*\)", lambda m: "1" if (m.group(1) in PREDEFINED or m.group(1) in macros) else "0", e)
        e = re.sub(r"defined\s+(\w+)", lambda m: "1" if (m.group(1) in PREDEFINED or m.group(1) in macros) else "0", e)
        # remaining identifiers -> their macro value if simple number, else 0
        def ident(m):
            name = m.group(0)
            if name in ("and", "or", "not"): return name
            d = macros.get(name)
            if d and d.params is None:
                b = d.body.strip()
                if re.fullmatch(r"0[xX][0-9a-fA-F]+|\d+", b): return b
            return "0"
        e = re.sub(r"\b[A-Za-z_]\w*\b", ident, e)
        e = e.replace("&&", " and ").replace("||", " or ").replace("!", " not ")
        e = e.replace(" not =", " !=")  # repair != damaged by ! replace
        try:
            return bool(eval(e))
        except Exception:
            return False
    for raw in text.split("\n"):
        line = raw.strip()
        if not line.startswith("#"):
            continue
        m = re.match(r"#\s*(\w+)\s*(.*)", line)
        if not m: continue
        directive, rest = m.group(1), m.group(2)
        if directive == "ifdef":
            name = rest.split()[0] if rest.split() else ""
            val = (name in PREDEFINED or name in macros) and active()
            cond_stack.append([val, val])
        elif directive == "ifndef":
            name = rest.split()[0] if rest.split() else ""
            val = (name not in PREDEFINED and name not in macros) and active()
            cond_stack.append([val, val])
        elif directive == "if":
            val = active() and eval_cond(rest)
            cond_stack.append([val, val])
        elif directive == "elif":
            if cond_stack:
                top = cond_stack[-1]
                below = all(c[0] for c in cond_stack[:-1])
                if top[1]:
                    top[0] = False
                else:
                    top[0] = below and eval_cond(rest)
                    top[1] = top[1] or top[0]
        elif directive == "else":
            if cond_stack:
                top = cond_stack[-1]
                below = all(c[0] for c in cond_stack[:-1])
                top[0] = (not top[1]) and below
                top[1] = top[1] or top[0]
        elif directive == "endif":
            if cond_stack: cond_stack.pop()
        elif directive == "define" and active():
            dm = re.match(r"(\w+)(\(([^)]*)\))?\s*(.*)", rest, flags=re.S)
            if dm:
                name = dm.group(1)
                params = None
                if dm.group(2) is not None:
                    params = [p.strip() for p in dm.group(3).split(",")] if dm.group(3).strip() else []
                macros[name] = MacroDef(params, dm.group(4).strip())
        elif directive == "undef" and active():
            name = rest.split()[0] if rest.split() else ""
            macros.pop(name, None)
    return macros

MBI_H = os.path.join(os.path.dirname(GBI_H), "mbi.h")

def load_all_macros():
    macros = load_gbi_macros(MBI_H)
    gbi = load_gbi_macros(GBI_H)
    macros.update(gbi)
    return macros

MACROS = load_all_macros()

def _split_args(s):
    args, depth, cur = [], 0, ""
    for ch in s:
        if ch == "," and depth == 0:
            args.append(cur.strip()); cur = ""
        else:
            if ch in "([{": depth += 1
            elif ch in ")]}": depth -= 1
            cur += ch
    if cur.strip() or args:
        args.append(cur.strip())
    return args

def expand_macros(expr, depth=0):
    if depth > 60:
        raise RuntimeError("macro recursion: " + expr[:120])
    out = expr
    changed = True
    while changed:
        changed = False
        i = 0
        res = []
        n = len(out)
        while i < n:
            m = re.match(r"[A-Za-z_]\w*", out[i:])
            if not m:
                res.append(out[i]); i += 1; continue
            name = m.group(0)
            j = i + len(name)
            d = MACROS.get(name)
            if d is None:
                res.append(name); i = j; continue
            if d.params is None:
                res.append(("(" + d.body + ")") if d.body.strip() else "")
                i = j; changed = True; continue
            # function-like: need parens
            k = j
            while k < n and out[k] in " \t": k += 1
            if k >= n or out[k] != "(":
                res.append(name); i = j; continue
            # find matching close
            depth_p, k2 = 0, k
            while k2 < n:
                if out[k2] == "(": depth_p += 1
                elif out[k2] == ")":
                    depth_p -= 1
                    if depth_p == 0: break
                k2 += 1
            args = _split_args(out[k+1:k2])
            body = d.body
            # handle token pasting first: param##x / x##param
            for pi, pn in enumerate(d.params):
                if pi < len(args):
                    body = re.sub(r"##\s*\b%s\b" % re.escape(pn), "##" + args[pi], body)
                    body = re.sub(r"\b%s\b\s*##" % re.escape(pn), args[pi] + "##", body)
            body = re.sub(r"\s*##\s*", "", body)
            for pi, pn in enumerate(d.params):
                if pi < len(args):
                    body = re.sub(r"\b%s\b" % re.escape(pn), "(" + args[pi] + ")", body)
            res.append(("(" + body + ")") if body.strip() else "")
            i = k2 + 1
            changed = True
        out = "".join(res)
    return out

# --- tiny C constant-expression evaluator (with ?:) ---
class ExprEval:
    def __init__(self, s):
        # strip casts and suffixes; resolve sizeofs (N64 struct sizes)
        s = re.sub(r"sizeof\s*\(\s*Mtx\s*\)", "64", s)
        s = re.sub(r"sizeof\s*\(\s*Vtx\s*\)", "16", s)
        s = re.sub(r"sizeof\s*\(\s*Gfx\s*\)", "8", s)
        s = re.sub(r"\(\s*(u?int(?:8|16|32|64)?_t|unsigned(?:\s+(?:int|long|char|short))?|int|long|char|short|u8|u16|u32|u64|s8|s16|s32|s64|uintptr_t|size_t)\s*\)", "", s)
        s = re.sub(r"(?<=[0-9a-fA-Fx])[uUlL]+\b", "", s)
        self.toks = re.findall(r"0[xX][0-9a-fA-F]+|\d+|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%()~!&|^<>?:]", s)
        leftover = re.sub(r"0[xX][0-9a-fA-F]+|\d+|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%()~!&|^<>?:]|\s+", "", s)
        if leftover:
            raise ValueError("unresolved tokens %r in %r" % (leftover, s[:200]))
        self.i = 0
    def peek(self):
        return self.toks[self.i] if self.i < len(self.toks) else None
    def next(self):
        t = self.peek(); self.i += 1; return t
    def parse(self):
        v = self.ternary()
        if self.peek() is not None:
            raise ValueError("trailing tokens")
        return v
    def ternary(self):
        c = self.lor()
        if self.peek() == "?":
            self.next(); a = self.ternary()
            assert self.next() == ":"
            b = self.ternary()
            return a if c else b
        return c
    def lor(self):
        v = self.land()
        while self.peek() == "||":
            self.next(); r = self.land(); v = 1 if (v or r) else 0
        return v
    def land(self):
        v = self.bor()
        while self.peek() == "&&":
            self.next(); r = self.bor(); v = 1 if (v and r) else 0
        return v
    def bor(self):
        v = self.bxor()
        while self.peek() == "|":
            self.next(); v |= self.bxor()
        return v
    def bxor(self):
        v = self.band()
        while self.peek() == "^":
            self.next(); v ^= self.band()
        return v
    def band(self):
        v = self.eq()
        while self.peek() == "&":
            self.next(); v &= self.eq()
        return v
    def eq(self):
        v = self.rel()
        while self.peek() in ("==", "!="):
            op = self.next(); r = self.rel()
            v = int(v == r) if op == "==" else int(v != r)
        return v
    def rel(self):
        v = self.shift()
        while self.peek() in ("<", ">", "<=", ">="):
            op = self.next(); r = self.shift()
            v = int({"<": v < r, ">": v > r, "<=": v <= r, ">=": v >= r}[op])
        return v
    def shift(self):
        v = self.add()
        while self.peek() in ("<<", ">>"):
            op = self.next(); r = self.add()
            v = (v << r) if op == "<<" else (v >> r)
        return v
    def add(self):
        v = self.mul()
        while self.peek() in ("+", "-"):
            op = self.next(); r = self.mul()
            v = v + r if op == "+" else v - r
        return v
    def mul(self):
        v = self.unary()
        while self.peek() in ("*", "/", "%"):
            op = self.next(); r = self.unary()
            v = v * r if op == "*" else (v // r if op == "/" else v % r)
        return v
    def unary(self):
        t = self.peek()
        if t == "-": self.next(); return -self.unary()
        if t == "+": self.next(); return self.unary()
        if t == "~": self.next(); return ~self.unary()
        if t == "!": self.next(); return int(not self.unary())
        if t == "(":
            self.next(); v = self.ternary()
            assert self.next() == ")"
            return v
        t = self.next()
        if t is None: raise ValueError("unexpected end")
        return int(t, 0)

def ceval(expr):
    return ExprEval(expand_macros(expr)).parse()

def eval_gfx_macro(call_text):
    """Expand a full gs*() invocation to one {w0, w1} pair. Returns (w0, w1)."""
    exp = expand_macros(call_text)
    exp = exp.strip()
    # expansion of gsXXX is {w0expr, w1expr} possibly wrapped in parens
    while exp.startswith("(") and exp.endswith(")"):
        # check the parens are balanced-wrapping
        depth = 0; ok = True
        for i, ch in enumerate(exp):
            if ch == "(": depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0 and i != len(exp) - 1: ok = False; break
        if ok: exp = exp[1:-1].strip()
        else: break
    if not (exp.startswith("{") and exp.endswith("}")):
        raise ValueError("macro did not expand to initializer: %s -> %s" % (call_text[:80], exp[:120]))
    inner = exp[1:-1]
    parts = _split_args(inner)
    if len(parts) != 2:
        raise ValueError("expected 2 words, got %d for %s" % (len(parts), call_text[:80]))
    w0 = ExprEval(parts[0]).parse() & 0xFFFFFFFF
    w1 = ExprEval(parts[1]).parse() & 0xFFFFFFFF
    return w0, w1

# ---------------------------------------------------------------------------
# C source parsing
# ---------------------------------------------------------------------------
def parse_c_arrays(text):
    """Return dict name -> (kind, payload):
       u64 tex  -> ('u64', bytes)
       Vtx      -> ('vtx', [ (x,y,z,f,s,t,r,g,b,a), ... ])
       Gfx      -> ('gfx', [ 'gsMacro(args)', ... ])
    """
    text = _strip_comments(text)
    out = {}
    for m in re.finditer(
        r"^(u64|Vtx|Gfx)\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{(.*?)\};",
        text, flags=re.S | re.M):
        kind, name, body = m.group(1), m.group(2), m.group(3)
        if kind == "u64":
            vals = re.findall(r"0[xX][0-9a-fA-F]+", body)
            data = b"".join(struct.pack(">Q", int(v, 16)) for v in vals)
            out[name] = ("u64", data)
        elif kind == "Vtx":
            nums = [int(x) for x in re.findall(r"-?\d+", body)]
            assert len(nums) % 10 == 0, "Vtx %s: %d numbers" % (name, len(nums))
            verts = [tuple(nums[i:i+10]) for i in range(0, len(nums), 10)]
            out[name] = ("vtx", verts)
        else:
            calls = []
            depth = 0; cur = ""
            for ch in body:
                cur += ch
                if ch == "(": depth += 1
                elif ch == ")":
                    depth -= 1
                    if depth == 0:
                        calls.append(cur.strip().lstrip(",""\n\t "))
                        cur = ""
            calls = [c.strip().lstrip(",").strip() for c in calls if c.strip().lstrip(",").strip()]
            out[name] = ("gfx", calls)
    return out

def parse_limbs_and_skel(text):
    text = _strip_comments(text)
    limbs = []
    for m in re.finditer(
        r"StandardLimb\s+(\w+)\s*=\s*\{\s*\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\}\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*([\w]+)\s*\}", text):
        limbs.append({
            "name": m.group(1),
            "pos": (int(m.group(2)), int(m.group(3)), int(m.group(4))),
            "child": int(m.group(5)), "sibling": int(m.group(6)),
            "dlist": None if m.group(7) == "NULL" else m.group(7),
        })
    mt = re.search(r"void\*\s+(\w+)\s*\[\s*(\d+)\s*\]\s*=\s*\{(.*?)\};", text, flags=re.S)
    limb_table_name, limb_table = None, []
    if mt:
        limb_table_name = mt.group(1)
        limb_table = re.findall(r"&\s*(\w+)", mt.group(3))
    sk = re.search(r"FlexSkeletonHeader\s+(\w+)\s*=\s*\{\s*\{?\s*(\w+)\s*,\s*(\d+)\s*\}?\s*,\s*(\d+)\s*\}", text)
    skel = None
    if sk:
        skel = {"name": sk.group(1), "table": sk.group(2),
                "limbCount": int(sk.group(3)), "dListCount": int(sk.group(4))}
    return limbs, limb_table_name, limb_table, skel

# ---------------------------------------------------------------------------
# Binary resource writers
# ---------------------------------------------------------------------------
def otr_header(fourcc: str, version: int = 0) -> bytes:
    h = struct.pack("<BBBB", 0, 0, 0, 0)              # little endian, is_custom=0
    h += struct.pack("<I", int.from_bytes(fourcc.encode(), "big"))
    h += struct.pack("<I", version)
    h += struct.pack("<Q", 0xDEADBEEFDEADBEEF)
    h += b"\x00" * (0x40 - len(h))
    return h

TEXTYPE_BY_SUFFIX = {
    "rgba32": 1, "rgba16": 2, "ci4": 3, "ci8": 4,
    "i4": 5, "i8": 6, "ia4": 7, "ia8": 8, "ia16": 9,
}
BPP_BY_SUFFIX = {"rgba32": 32, "rgba16": 16, "ci4": 4, "ci8": 8,
                 "i4": 4, "i8": 8, "ia4": 4, "ia8": 8, "ia16": 16}

def tex_suffix(name):
    m = re.search(r"_(rgba32|rgba16|ci4|ci8|ia16|ia8|ia4|i8|i4)$", name)
    return m.group(1) if m else None

def build_texture(name, data, dims):
    suf = tex_suffix(name)
    assert suf, "texture %s has no format suffix" % name
    ttype = TEXTYPE_BY_SUFFIX[suf]
    bpp = BPP_BY_SUFFIX[suf]
    if dims:
        w, h = dims
    else:
        px = len(data) * 8 // bpp
        w = 32 if px % 32 == 0 else 16
        h = max(1, px // w)
    body = struct.pack("<IIII", ttype, w, h, len(data)) + data
    return otr_header("OTEX", 0) + body

def build_vtx(verts):
    body = struct.pack("<II", 25, len(verts))  # ArrayResourceType::Vertex
    for (x, y, z, f, s, t, r, g, b, a) in verts:
        body += struct.pack("<hhhHhhBBBB", x, y, z, f & 0xFFFF, s, t,
                            r & 0xFF, g & 0xFF, b & 0xFF, a & 0xFF)
    return otr_header("OARR", 0) + body

# ---------------------------------------------------------------------------
# Gfx array -> ODLT
# ---------------------------------------------------------------------------
class DLBuilder:
    def __init__(self, symtab, respath_of):
        self.symtab = symtab          # symbol -> (kind, payload)
        self.respath_of = respath_of  # symbol -> archive path

    def encode(self, name, calls):
        words = []
        # ZAPD-style debug marker (0x33 is a 128-bit no-op in the interpreter)
        h = crc64(self.respath_of(name))
        words += [(0x33 << 24, 0xBEEFBEEF), ((h >> 32) & 0xFFFFFFFF, h & 0xFFFFFFFF)]
        for call in calls:
            words += self.encode_call(name, call)
        data = b""
        for (w0, w1) in words:
            data += struct.pack("<II", w0 & 0xFFFFFFFF, w1 & 0xFFFFFFFF)
        body = struct.pack("<B", 4)  # ucode_f3dex2
        body += b"\x00" * ((8 - (len(body) % 8)) % 8)
        return otr_header("ODLT", 0) + body + data

    def hash_pair(self, sym):
        p = self.respath_of(sym)
        h = crc64(p)
        return ((h >> 32) & 0xFFFFFFFF, h & 0xFFFFFFFF)

    def encode_call(self, dlname, call):
        m = re.match(r"(\w+)\s*\((.*)\)\s*$", call, flags=re.S)
        if not m:
            raise ValueError("bad gfx call in %s: %r" % (dlname, call[:80]))
        fn, argstr = m.group(1), m.group(2)
        args = _split_args(argstr) if argstr.strip() else []

        if fn == "gsSPDisplayList":
            sym = args[0].strip()
            if re.fullmatch(r"0[xX][0-9a-fA-F]+|\d+", sym):
                # segmented branch (e.g. 0x0C000000 set by the actor at draw time)
                addr = ceval(sym)
                seg = (addr >> 24) & 0xFF
                w1 = (addr & 0x0FFFFFFF) + 1 if 0x01 <= seg <= 0x0F else addr
                return [((0xDE << 24), w1)]
            assert re.fullmatch(r"\w+", sym), "gsSPDisplayList arg %r" % sym
            return [((0x31 << 24), 0), self.hash_pair(sym)]

        if fn == "gsSPVertex":
            tgt = args[0].strip()
            mm2 = re.fullmatch(r"(\w+)\s*(?:\+\s*(\d+))?", tgt)
            assert mm2, "gsSPVertex arg %r" % tgt
            sym, off = mm2.group(1), int(mm2.group(2) or 0)
            n = ceval(args[1]); v0 = ceval(args[2])
            w0, _ = eval_gfx_macro("gsSPVertex(0, %d, %d)" % (n, v0))
            w0 = (w0 & 0x00FFFFFF) | (0x32 << 24)
            byte_off = off * 16
            assert byte_off <= 0xFFFFF
            return [(w0, byte_off), self.hash_pair(sym)]

        if fn == "gsDPSetTextureImage":
            img = args[3].strip()
            if re.fullmatch(r"\w+", img) and not re.fullmatch(r"0[xX][0-9a-fA-F]+|\d+", img):
                # symbol -> OTR hash settimg
                w0, _ = eval_gfx_macro("gsDPSetTextureImage(%s, %s, %s, 0)" %
                                       (args[0], args[1], args[2]))
                w0 = (w0 & 0x00FFFFFF) | (0x20 << 24)
                return [(w0, 0), self.hash_pair(img)]
            # numeric (segment) address: standard command + odd segment marker
            w0, w1 = eval_gfx_macro("gsDPSetTextureImage(%s, %s, %s, 0)" %
                                    (args[0], args[1], args[2]))
            addr = ceval(img)
            seg = (addr >> 24) & 0xFF
            if 0x01 <= seg <= 0x0F:
                w1 = (addr & 0x0FFFFFFF) + 1
            else:
                w1 = addr
            return [(w0, w1)]

        if fn == "gsSPMatrix":
            w0, w1 = eval_gfx_macro(call)
            seg = (w1 >> 24) & 0xFF
            if 0x01 <= seg <= 0x0F and (w1 & 1) == 0:
                w1 = (w1 & 0x0FFFFFFF) + 1
            return [(w0, w1)]

        # generic: expand real gbi macro
        return [eval_gfx_macro(call)]

# ---------------------------------------------------------------------------
# per-enemy processing
# ---------------------------------------------------------------------------
def gather_object(folder, objname, skelbase):
    objdir = os.path.join(folder, objname)
    files = sorted(os.listdir(objdir))
    skel_file = None
    anim_files = []
    for f in files:
        if not f.endswith(".c"): continue
        if f == objname + ".c": continue
        if f == skelbase + ".c":
            skel_file = os.path.join(objdir, f)
        elif "Anim" in f:
            anim_files.append(os.path.join(objdir, f))
    assert skel_file, "no skel file for " + objname
    skel_text = open(skel_file, encoding="utf-8", errors="replace").read()
    arrays = parse_c_arrays(skel_text)
    limbs, table_name, table, skel = parse_limbs_and_skel(skel_text)
    return {
        "folder": folder, "objname": objname, "skelbase": skelbase,
        "objdir": objdir, "arrays": arrays, "limbs": limbs,
        "table_name": table_name, "table": table, "skel": skel,
        "anim_files": anim_files, "skel_text": skel_text,
    }

def texture_dims_from_dls(arrays):
    """Walk all gfx arrays; map texture symbol -> (w,h) using SetTileSize /
    LoadTLUTCmd following the settimg that referenced it."""
    dims = {}
    for name, (kind, payload) in arrays.items():
        if kind != "gfx": continue
        last_img = None
        for call in payload:
            m = re.match(r"(\w+)\s*\((.*)\)\s*$", call, flags=re.S)
            if not m: continue
            fn, argstr = m.group(1), m.group(2)
            args = _split_args(argstr)
            if fn == "gsDPSetTextureImage":
                img = args[3].strip()
                last_img = img if re.fullmatch(r"\w+", img) and not img[0].isdigit() else None
            elif fn == "gsDPLoadTLUTCmd" and last_img:
                count = ceval(args[1]) + 1
                dims[last_img] = (count, 1)
                last_img = None
            elif fn == "gsDPSetTileSize" and last_img:
                lrs = ceval(args[3]); lrt = ceval(args[4])
                dims[last_img] = ((lrs >> 2) + 1, (lrt >> 2) + 1)
                last_img = None
    return dims

def build_all():
    os.makedirs(OUT_DIR, exist_ok=True)
    zf = zipfile.ZipFile(O2R_PATH, "w", zipfile.ZIP_DEFLATED)
    all_paths = []
    gen_dir = os.path.join(OUT_DIR, "gen")
    os.makedirs(gen_dir, exist_ok=True)
    report = {}

    for folder, objname, skelbase in ENEMIES:
        info = gather_object(folder, objname, skelbase)
        arrays = info["arrays"]
        base = "%s/%s" % (OTR_PREFIX, objname)
        def respath_of(sym):
            return "%s/%s" % (base, sym)

        dims = texture_dims_from_dls(arrays)
        dlb = DLBuilder(arrays, respath_of)

        n_tex = n_vtx = n_dl = 0
        for name, (kind, payload) in arrays.items():
            path = respath_of(name)
            if kind == "u64":
                zf.writestr(path, build_texture(name, payload, dims.get(name)))
                n_tex += 1
            elif kind == "vtx":
                zf.writestr(path, build_vtx(payload))
                n_vtx += 1
            elif kind == "gfx":
                zf.writestr(path, dlb.encode(name, payload))
                n_dl += 1
            all_paths.append(path)

        report[objname] = {"textures": n_tex, "vtx": n_vtx, "dls": n_dl,
                           "limbs": len(info["limbs"]),
                           "anims": len(info["anim_files"])}
        emit_assets_c(info, gen_dir, base)

    zf.close()
    return report, all_paths

# ---------------------------------------------------------------------------
# generated .inc.c / .h (compiles identically in soh & 2ship)
# ---------------------------------------------------------------------------
def emit_assets_c(info, gen_dir, base):
    objname = info["objname"]
    arrays = info["arrays"]
    out_c = os.path.join(gen_dir, objname + "_assets.inc.c")
    out_h = os.path.join(gen_dir, objname + "_assets.h")

    dl_syms = [n for n, (k, _) in arrays.items() if k == "gfx"]
    tex_syms = [n for n, (k, _) in arrays.items() if k == "u64"]

    # which DLs are referenced by limbs (only those need path refs)
    limb_dls = sorted({l["dlist"] for l in info["limbs"] if l["dlist"]})
    # textures referenced from actor code (eye/segment textures = not referenced by any DL settimg)
    referenced = set()
    for n, (k, payload) in arrays.items():
        if k != "gfx": continue
        for call in payload:
            for w in re.findall(r"\w+", call):
                referenced.add(w)
    actor_texs = [t for t in tex_syms if t not in referenced]

    L = []
    L.append("/* AUTO-GENERATED by build_trutefel_o2r.py — do not edit by hand.")
    L.append("   Compiled skeleton + animations for %s; meshes/textures live in" % objname)
    L.append("   trutefel-enemies.o2r under __OTR__%s/. Same file compiles in soh and 2ship. */" % base)
    L.append("")
    for dl in limb_dls:
        L.append('static const ALIGN_ASSET(2) char %s_Ref[] = "__OTR__%s/%s";' % (dl, base, dl))
    L.append("")
    for t in actor_texs:
        L.append('const ALIGN_ASSET(2) char %s[] = "__OTR__%s/%s";' % (t, base, t))
    L.append("")
    for l in info["limbs"]:
        dl = "NULL" if not l["dlist"] else "(Gfx*)%s_Ref" % l["dlist"]
        L.append("StandardLimb %s = { { %d, %d, %d }, %d, %d, %s };" %
                 (l["name"], l["pos"][0], l["pos"][1], l["pos"][2],
                  l["child"], l["sibling"], dl))
    L.append("")
    L.append("void* %s[%d] = {" % (info["table_name"], len(info["table"])))
    for t in info["table"]:
        L.append("    &%s," % t)
    L.append("};")
    L.append("")
    sk = info["skel"]
    L.append("FlexSkeletonHeader %s = { { %s, %d }, %d };" %
             (sk["name"], sk["table"], sk["limbCount"], sk["dListCount"]))
    L.append("")

    # animations: passthrough minus includes
    anim_headers = []
    for af in info["anim_files"]:
        text = open(af, encoding="utf-8", errors="replace").read()
        text = _strip_comments(text)
        text = re.sub(r'#include[^\n]*\n', "", text)
        L.append("/* ---- %s ---- */" % os.path.basename(af))
        L.append(text.strip())
        L.append("")
        for m in re.finditer(r"AnimationHeader\s+(\w+)\s*=", text):
            anim_headers.append(m.group(1))

    open(out_c, "w", encoding="utf-8", newline="\n").write("\n".join(L))

    # header: limb enum copied from original skel header + externs
    H = []
    guard = "TRUTEFEL_%s_ASSETS_H" % objname.upper()
    H.append("#ifndef %s" % guard)
    H.append("#define %s" % guard)
    H.append("")
    orig_h = os.path.join(info["objdir"], info["skelbase"] + ".h")
    enum_txt = ""
    if os.path.exists(orig_h):
        ht = _strip_comments(open(orig_h, encoding="utf-8", errors="replace").read())
        em = re.search(r"typedef\s+enum\s*\{.*?\}\s*\w+\s*;", ht, flags=re.S)
        if em:
            enum_txt = em.group(0)
        for dm in re.finditer(r"#define\s+(\w+)\s+(\d+)\s*$", ht, flags=re.M):
            H.append("#define %s %s" % (dm.group(1), dm.group(2)))
    if enum_txt:
        H.append(enum_txt)
    H.append("")
    H.append("extern FlexSkeletonHeader %s;" % sk["name"])
    for a in anim_headers:
        H.append("extern AnimationHeader %s;" % a)
    for t in actor_texs:
        H.append("extern const char %s[];" % t)
    H.append("")
    H.append("#endif")
    open(out_h, "w", encoding="utf-8", newline="\n").write("\n".join(H))

# ---------------------------------------------------------------------------
# validation: decode-walk every ODLT in the archive
# ---------------------------------------------------------------------------
EXPANDED_OPS = {0x20, 0x24, 0x25, 0x27, 0x29, 0x31, 0x32, 0x33, 0x35, 0x36, 0x42}
KNOWN_OPS = {0x00, 0x01, 0x05, 0x06, 0x07, 0xD7, 0xD9, 0xDA, 0xDE, 0xDF,
             0xE2, 0xE3, 0xE7, 0xF0, 0xF2, 0xF3, 0xF5, 0xFA, 0xFC, 0xFD} | EXPANDED_OPS

def validate():
    zf = zipfile.ZipFile(O2R_PATH)
    names = zf.namelist()
    hashmap = {crc64(n): n for n in names}
    problems = []
    vtx_counts = {}
    def fourcc(raw):
        return struct.unpack_from("<I", raw, 4)[0].to_bytes(4, "big")
    for n in names:
        raw = zf.read(n)
        if fourcc(raw) == b"OARR":
            at, cnt = struct.unpack_from("<II", raw, 0x40)
            assert at == 25
            vtx_counts[n] = cnt
    n_dl = 0
    for n in names:
        raw = zf.read(n)
        if fourcc(raw) != b"ODLT": continue
        n_dl += 1
        ucode = raw[0x40]
        assert ucode == 4, n
        pos = 0x48
        end_seen = False
        while pos + 8 <= len(raw):
            w0, w1 = struct.unpack_from("<II", raw, pos)
            op = (w0 >> 24) & 0xFF
            pos += 8
            if op not in KNOWN_OPS:
                problems.append("%s: unknown opcode %02X" % (n, op)); break
            if op in EXPANDED_OPS:
                h0, h1 = struct.unpack_from("<II", raw, pos)
                pos += 8
                if op in (0x20, 0x31, 0x32):
                    h = (h0 << 32) | h1
                    tgt = hashmap.get(h)
                    if tgt is None:
                        problems.append("%s: dangling hash op %02X %016X" % (n, op, h))
                    elif op == 0x32:
                        cnt = vtx_counts.get(tgt)
                        nverts = (w0 >> 12) & 0xFF
                        off = w1
                        if cnt is None:
                            problems.append("%s: vtx ref to non-OARR %s" % (n, tgt))
                        elif off % 16 != 0 or off // 16 + nverts > cnt:
                            problems.append("%s: vtx overrun %s off=%d n=%d cnt=%d" %
                                            (n, tgt, off, nverts, cnt))
            if op == 0xDF:
                end_seen = True
                if pos != len(raw):
                    problems.append("%s: trailing bytes after ENDDL" % n)
                break
        if not end_seen:
            problems.append("%s: no ENDDL" % n)
    zf.close()
    return problems, n_dl

if __name__ == "__main__":
    report, paths = build_all()
    print(json.dumps(report, indent=1))
    print("total resources:", len(paths))
    problems, n_dl = validate()
    print("validated %d DLs" % n_dl)
    if problems:
        print("PROBLEMS:")
        for p in problems[:40]:
            print("  " + p)
        sys.exit(1)
    print("OK:", O2R_PATH)
