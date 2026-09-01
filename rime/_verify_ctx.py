# -*- coding: utf-8 -*-
"""End-to-end verification of the sino_mix (Korean) dictionary via Rime native API.
Uses exact struct layouts from rime_api.h. Proves "type pinyin -> Korean candidate appears".
"""
import ctypes

DLL = r"C:\Program Files\Rime\weasel-0.17.4\rime.dll"
SHARED = r"C:\Program Files\Rime\weasel-0.17.4\data".encode("utf-8")
USER = r"C:\Users\Administrator\AppData\Roaming\Rime".encode("utf-8")

# ---- structs (mirror rime_api.h) ----
class RimeTraits(ctypes.Structure):
    _fields_ = [
        ("data_size", ctypes.c_int),
        ("shared_data_dir", ctypes.c_char_p),
        ("user_data_dir", ctypes.c_char_p),
        ("distribution_name", ctypes.c_char_p),
        ("distribution_code_name", ctypes.c_char_p),
        ("distribution_version", ctypes.c_char_p),
        ("app_name", ctypes.c_char_p),
        ("modules", ctypes.POINTER(ctypes.c_char_p)),
        ("min_log_level", ctypes.c_int),
        ("log_dir", ctypes.c_char_p),
        ("prebuilt_data_dir", ctypes.c_char_p),
        ("staging_dir", ctypes.c_char_p),
    ]

class RimeCandidate(ctypes.Structure):
    _fields_ = [
        ("text", ctypes.c_char_p),
        ("comment", ctypes.c_char_p),
        ("reserved", ctypes.c_void_p),
    ]

class RimeMenu(ctypes.Structure):
    _fields_ = [
        ("page_size", ctypes.c_int),
        ("page_no", ctypes.c_int),
        ("is_last_page", ctypes.c_int),
        ("highlighted_candidate_index", ctypes.c_int),
        ("num_candidates", ctypes.c_int),
        ("candidates", ctypes.POINTER(RimeCandidate)),
        ("select_keys", ctypes.c_char_p),
    ]

class RimeComposition(ctypes.Structure):
    _fields_ = [
        ("length", ctypes.c_int),
        ("cursor_pos", ctypes.c_int),
        ("sel_start", ctypes.c_int),
        ("sel_end", ctypes.c_int),
        ("preedit", ctypes.c_char_p),
    ]

class RimeContext(ctypes.Structure):
    _fields_ = [
        ("data_size", ctypes.c_int),
        ("composition", RimeComposition),
        ("menu", RimeMenu),
        ("commit_text_preview", ctypes.c_char_p),
        ("select_labels", ctypes.POINTER(ctypes.c_char_p)),
    ]

dll = ctypes.CDLL(DLL)
dll.RimeInitialize.argtypes = [ctypes.POINTER(RimeTraits)]
dll.RimeInitialize.restype = ctypes.c_int
dll.RimeCreateSession.argtypes = []
dll.RimeCreateSession.restype = ctypes.c_uint64
dll.RimeSelectSchema.argtypes = [ctypes.c_uint64, ctypes.c_char_p]
dll.RimeSelectSchema.restype = ctypes.c_int
dll.RimeSimulateKeySequence.argtypes = [ctypes.c_uint64, ctypes.c_char_p]
dll.RimeSimulateKeySequence.restype = ctypes.c_int
dll.RimeGetContext.argtypes = [ctypes.c_uint64, ctypes.POINTER(RimeContext)]
dll.RimeGetContext.restype = ctypes.c_int
dll.RimeFreeContext.argtypes = [ctypes.POINTER(RimeContext)]
dll.RimeFreeContext.restype = None
dll.RimeClearComposition.argtypes = [ctypes.c_uint64]
dll.RimeClearComposition.restype = None
dll.RimeFinalize.argtypes = []
dll.RimeFinalize.restype = None

def main():
    tr = RimeTraits()
    tr.data_size = ctypes.sizeof(RimeTraits) - ctypes.sizeof(ctypes.c_int)
    tr.shared_data_dir = SHARED
    tr.user_data_dir = USER
    tr.app_name = b"rime.test"
    tr.distribution_name = b"Rime"
    tr.distribution_code_name = b"weasel"
    tr.distribution_version = b"0.17.4"
    ok = dll.RimeInitialize(ctypes.byref(tr))
    print("RimeInitialize ->", ok)
    sid = dll.RimeCreateSession()
    print("create_session ->", sid)
    sel = dll.RimeSelectSchema(sid, b"sino_mix")
    print("select_schema(sino_mix) ->", sel)

    tests = ["nihao", "zhongdan", "daye", "tuanzhan", "male", "jile",
             "xiaole", "dazhao", "bangmang", "bengbuzhu", "juele",
             "wocao", "xialu", "fuzhu", "sile", "touxiang", "lihao"]
    for seq in tests:
        dll.RimeSimulateKeySequence(sid, seq.encode("utf-8"))
        ctx = RimeContext()
        ctx.data_size = ctypes.sizeof(RimeContext) - ctypes.sizeof(ctypes.c_int)
        got = dll.RimeGetContext(sid, ctypes.byref(ctx))
        preedit = ctx.composition.preedit.decode("utf-8", "replace") if ctx.composition.preedit else ""
        n = ctx.menu.num_candidates
        cands = []
        if ctx.menu.candidates and n > 0:
            arr = ctypes.cast(ctx.menu.candidates, ctypes.POINTER(RimeCandidate * n))[0]
            for i in range(n):
                t = arr[i].text
                cands.append(t.decode("utf-8", "replace") if t else "")
        print(f"[{seq}] get_ctx={got} preedit='{preedit}' num_candidates={n}")
        print("    candidates:", " | ".join(cands))
        # clear composition for next test (proper API, not a key string)
        dll.RimeClearComposition(sid)

    dll.RimeFinalize()

if __name__ == "__main__":
    main()
