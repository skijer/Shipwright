"""Small headless compatibility OpenGL context, using system Mesa/EGL only."""
import ctypes as C
import os
import numpy as np
os.environ.setdefault('EGL_PLATFORM','surfaceless')
E=C.CDLL('libEGL.so.1'); G=C.CDLL('libGL.so.1')
def fn(lib,name,restype,*args):
 f=getattr(lib,name);f.restype=restype;f.argtypes=args;return f
ptr=C.c_void_p;integer=C.c_int;uint=C.c_uint

def context(w,h):
 display=fn(E,'eglGetDisplay',ptr,ptr)(None)
 major,minor=integer(),integer()
 assert fn(E,'eglInitialize',uint,ptr,C.POINTER(integer),C.POINTER(integer))(display,C.byref(major),C.byref(minor))
 assert fn(E,'eglBindAPI',uint,uint)(0x30A2)
 attrs=(integer*15)(0x3033,1,0x3040,8,0x3024,8,0x3023,8,0x3022,8,0x3025,24,0x3021,8,0x3038)
 config=ptr();n=integer()
 assert fn(E,'eglChooseConfig',uint,ptr,C.POINTER(integer),C.POINTER(ptr),integer,C.POINTER(integer))(display,attrs,C.byref(config),1,C.byref(n)) and n.value
 surf=fn(E,'eglCreatePbufferSurface',ptr,ptr,ptr,C.POINTER(integer))(display,config,(integer*5)(0x3057,w,0x3056,h,0x3038))
 ctx=fn(E,'eglCreateContext',ptr,ptr,ptr,ptr,C.POINTER(integer))(display,config,None,(integer*1)(0x3038))
 assert ctx and surf and fn(E,'eglMakeCurrent',uint,ptr,ptr,ptr,ptr)(display,surf,surf,ctx)
 return display,surf,ctx

def gl(name,restype,*args): return fn(G,name,restype,*args)
if __name__=='__main__':
 context(64,64)
 print(gl('glGetString',C.c_char_p,uint)(0x1F02).decode())
