"""Headless Mesa renderer for source-sampled player fitting evidence."""
import ctypes as C
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "nei_gi/runtime_preview"))
from gl_context import context, gl, ptr, integer, uint
from player_animation import transformed

F, D = C.c_float, C.c_double


class Renderer:
    def __init__(self, width=1440, height=840):
        self.width, self.height = width, height
        self.context = context(width, height)
        specs = {
            "ClearColor": (None, F, F, F, F), "Clear": (None, uint),
            "Enable": (None, uint), "Disable": (None, uint),
            "Viewport": (None, integer, integer, integer, integer),
            "MatrixMode": (None, uint), "LoadIdentity": (None,),
            "Ortho": (None, D, D, D, D, D, D), "Rotatef": (None, F, F, F, F),
            "Translatef": (None, F, F, F), "LineWidth": (None, F),
            "Begin": (None, uint), "End": (None,), "Vertex3f": (None, F, F, F),
            "Color4f": (None, F, F, F, F), "BlendFunc": (None, uint, uint),
            "DepthMask": (None, C.c_ubyte), "AlphaFunc": (None, uint, F),
            "GenTextures": (None, integer, C.POINTER(uint)), "BindTexture": (None, uint, uint),
            "TexParameteri": (None, uint, uint, integer),
            "TexImage2D": (None, uint, integer, integer, integer, integer, integer, uint, uint, ptr),
            "ReadPixels": (None, integer, integer, integer, integer, uint, uint, ptr),
            "EnableClientState": (None, uint), "DisableClientState": (None, uint),
            "VertexPointer": (None, integer, uint, integer, ptr),
            "ColorPointer": (None, integer, uint, integer, ptr),
            "TexCoordPointer": (None, integer, uint, integer, ptr),
            "DrawArrays": (None, uint, integer, integer),
        }
        for name, spec in specs.items(): setattr(self, name, gl("gl" + name, *spec))
        self.Enable(0x0B71); self.Enable(0x0BE2); self.BlendFunc(0x0302, 0x0303)
        self.Enable(0x0BC0); self.AlphaFunc(0x0204, .15)
        self.ClearColor(.055, .075, .105, 1)
        self.textures = {}
        self.light = np.array([-.4, .6, .7]); self.light /= np.linalg.norm(self.light)

    def clear(self):
        self.DepthMask(1); self.Clear(0x4000 | 0x0100)

    def view(self, x, y, width, height, yaw=25, pitch=8, center=(0, 24, 0), span=72):
        self.Viewport(x, y, width, height)
        self.MatrixMode(0x1701); self.LoadIdentity()
        half = span * width / height / 2
        self.Ortho(-half, half, -span/2, span/2, -1000, 1000)
        self.MatrixMode(0x1700); self.LoadIdentity()
        self.Rotatef(pitch, 1, 0, 0); self.Rotatef(yaw, 0, 1, 0)
        self.Translatef(*(-np.array(center)))

    def texture(self, pixels):
        key = id(pixels)
        if key not in self.textures:
            tex = uint(); self.GenTextures(1, C.byref(tex))
            self.textures[key] = tex.value
            self.BindTexture(0x0DE1, tex.value)
            for option in (0x2801, 0x2800): self.TexParameteri(0x0DE1, option, 0x2601)
            for option in (0x2802, 0x2803): self.TexParameteri(0x0DE1, option, 0x2901)
            data = np.ascontiguousarray(pixels)
            self.TexImage2D(0x0DE1, 0, 0x1908, data.shape[1], data.shape[0], 0, 0x1908, 0x1401, data.ctypes.data)
        return self.textures[key]

    def prepare(self, meshes, matrices):
        prepared = []
        for mesh in meshes:
            pos, normals = transformed(mesh, matrices)
            shade = .63 + .37 * np.maximum(normals @ self.light, 0)
            rgba = mesh.rgba.copy(); rgba[:, :3] *= shade[:, None]
            uv = np.c_[.5 + normals[:, 0]*.5, .5 - normals[:, 1]*.5] if mesh.texgen else mesh.uv
            packed = np.ascontiguousarray(np.c_[pos, rgba, uv], dtype=np.float32)
            prepared.append((mesh, packed))
        return prepared

    def draw(self, prepared):
        self.EnableClientState(0x8074); self.EnableClientState(0x8076); self.EnableClientState(0x8078)
        for mesh, packed in sorted(prepared, key=lambda pair: pair[0].transparent):
            self.DepthMask(0 if mesh.transparent else 1)
            (self.Enable if mesh.cull else self.Disable)(0x0B44)
            if mesh.texture is not None:
                self.Enable(0x0DE1); self.BindTexture(0x0DE1, self.texture(mesh.texture))
            else: self.Disable(0x0DE1)
            self.VertexPointer(3, 0x1406, 36, packed.ctypes.data)
            self.ColorPointer(4, 0x1406, 36, packed.ctypes.data+12)
            self.TexCoordPointer(2, 0x1406, 36, packed.ctypes.data+28)
            self.DrawArrays(0x0004, 0, len(packed))
        self.DisableClientState(0x8074); self.DisableClientState(0x8076); self.DisableClientState(0x8078)
        self.Disable(0x0DE1); self.DepthMask(1)

    def grid(self, y=0, size=45, step=10):
        self.Disable(0x0DE1); self.Disable(0x0B44); self.LineWidth(1)
        self.Color4f(.19, .24, .30, 1); self.Begin(0x0001)
        for v in np.arange(-size, size+.001, step):
            self.Vertex3f(v, y, -size); self.Vertex3f(v, y, size)
            self.Vertex3f(-size, y, v); self.Vertex3f(size, y, v)
        self.End()

    def axes(self, length=6):
        self.Disable(0x0DE1); self.LineWidth(2)
        self.Begin(0x0001)
        for point, color in [((length, 0, 0), (1, .3, .3)), ((0, length, 0), (.3, 1, .4)), ((0, 0, length), (.4, .5, 1))]:
            self.Color4f(*color, 1); self.Vertex3f(0, 0, 0); self.Vertex3f(*point)
        self.End()

    def image(self):
        data = np.zeros((self.height, self.width, 4), np.uint8)
        self.ReadPixels(0, 0, self.width, self.height, 0x1908, 0x1401, data.ctypes.data)
        return Image.fromarray(data[::-1].copy()).convert("RGB")
