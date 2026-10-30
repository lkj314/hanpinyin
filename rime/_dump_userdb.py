#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""只读解析 Rime userdb（LevelDB）并导出全部条目 —— T1-3 挖矿与本文档实验的基础工具。

用法：
    python _dump_userdb.py                 # 解析 %APPDATA%\\Rime\\hanpinyin.userdb（真实学习数据）
    python _dump_userdb.py <userdb_dir>    # 解析指定目录（如隔离实验产生的临时库）
    python _dump_userdb.py --raw <dir>     # 逐文件输出（默认输出合并视图：后写覆盖先写、删除生效）

只读保证：不写入、不加锁、不改名、不删任何文件；解析被输入法占用的库前建议先复制目录。
输出格式：每行  [来源] 键 => 值；键形如 "码 \\t 词"；值形如 "c=提交次数 d=权重 t=时间戳"。
  * \\x01 开头 = 库元数据（/tick 等）；\\x7fenc\\x1f 开头 = ☯ 编码词组条目（本库目前不应出现）。
"""
import os
import struct
import sys

REAL_USERDB = os.path.join(os.environ.get("APPDATA", ""), "Rime", "hanpinyin.userdb")


def _varint(buf, pos):
    result = shift = 0
    while True:
        b = buf[pos]; pos += 1
        result |= (b & 0x7F) << shift
        if not (b & 0x80):
            return result, pos
        shift += 7


def _snappy_decompress(data):
    """纯 Python snappy 解压（LevelDB 块压缩用），无需第三方库。"""
    ulen, pos = _varint(data, 0)
    out = bytearray()
    while pos < len(data):
        tag = data[pos]; pos += 1
        t = tag & 3
        if t == 0:  # literal
            ln = tag >> 2
            if ln >= 60:
                nbytes = ln - 59
                ln = int.from_bytes(data[pos:pos + nbytes], "little"); pos += nbytes
            ln += 1
            out += data[pos:pos + ln]; pos += ln
        else:       # copy
            if t == 1:
                ln = 4 + ((tag >> 2) & 7)
                off = data[pos]; pos += 1
            elif t == 2:
                ln = 1 + (tag >> 2)
                off = int.from_bytes(data[pos:pos + 2], "little"); pos += 2
            else:
                ln = 1 + (tag >> 2)
                off = int.from_bytes(data[pos:pos + 4], "little"); pos += 4
            for _ in range(ln):
                out.append(out[-off])
    if len(out) != ulen:
        raise ValueError(f"snappy 解压长度不符 {len(out)} != {ulen}")
    return bytes(out)


def _block_entries(content):
    """解析一个 LevelDB 块内的 (key, value) 序列（前缀压缩 + restart array）。"""
    if len(content) < 4:
        return []
    num_restarts = struct.unpack("<I", content[-4:])[0]
    stop = len(content) - 4 - num_restarts * 4
    entries, pos, key = [], 0, b""
    while pos < stop:
        shared, pos = _varint(content, pos)
        non_shared, pos = _varint(content, pos)
        vlen, pos = _varint(content, pos)
        key = key[:shared] + content[pos:pos + non_shared]; pos += non_shared
        val = content[pos:pos + vlen]; pos += vlen
        entries.append((key, val))
    return entries


def parse_ldb(path):
    """解析 .ldb（sstable）：footer -> index block -> data blocks。

    返回 [(user_key, value_or_None)]：剥离 8 字节内部键后缀（type+seq），
    同名用户键取首个出现（sstable 内按 seq 降序 = 最新版本在前）；
    最新版本为删除墓碑（type=0）时 value=None。
    """
    data = open(path, "rb").read()
    if len(data) < 48:
        return []
    foot = data[-48:]
    p = 0
    _mo, p = _varint(foot, p); _ms, p = _varint(foot, p)
    io, p = _varint(foot, p); isz, p = _varint(foot, p)
    index = data[io:io + isz]
    if data[io + isz] == 1:  # snappy
        index = _snappy_decompress(index)
    out, seen = [], set()
    for _k, handle in _block_entries(index):
        off, p2 = _varint(handle, 0)
        size, _p3 = _varint(handle, p2)
        content = data[off:off + size]
        if data[off + size] == 1:
            content = _snappy_decompress(content)
        for ikey, val in _block_entries(content):
            if len(ikey) < 9:
                continue
            ukey, itype = ikey[:-8], ikey[-8]
            if ukey in seen:
                continue  # 后续是同名键的旧版本
            seen.add(ukey)
            out.append((ukey, val if itype == 1 else None))
    return out


def parse_wal(path):
    """解析 .log（LevelDB WAL）：32KB 块 + WriteBatch，返回 [(op, key, value)]。"""
    data = open(path, "rb").read()
    records, pos, payload = [], 0, b""
    BLOCK = 32768
    while pos < len(data):
        block_end = min(pos - pos % BLOCK + BLOCK, len(data))
        if block_end - pos < 7:
            break
        _crc, ln, typ = struct.unpack("<IHB", data[pos:pos + 7])
        pos += 7
        if typ in (1, 2, 3, 4) and pos + ln <= block_end:
            chunk = data[pos:pos + ln]
            pos += ln
            if typ == 1:
                records.append(chunk)
            elif typ == 2:
                payload = chunk
                continue
            elif typ in (3, 4):
                payload += chunk
                if typ == 4:
                    records.append(payload)
            continue
        pos = block_end  # 坏记录，跳到下一块
    out = []
    for rec in records:
        if len(rec) < 12:
            continue
        _seq = struct.unpack("<Q", rec[:8])[0]
        cnt = struct.unpack("<I", rec[8:12])[0]
        p = 12
        for _ in range(cnt):
            tag = rec[p]; p += 1
            klen, p = _varint(rec, p)
            key = rec[p:p + klen]; p += klen
            if tag == 1:
                vlen, p = _varint(rec, p)
                val = rec[p:p + vlen]; p += vlen
            else:
                val = None  # 删除墓碑
            out.append(("DEL" if val is None else "PUT", key, val))
    return out


def _pretty(b):
    if b is None:
        return "<DELETE>"
    s = b.decode("utf-8", "replace")
    tag = ""
    if s.startswith("\x01"):
        tag = "[meta]"
    elif s.startswith("\x7fenc\x1f"):
        tag = "[☯编码]"
    s = s.replace("\x01", "\\x01").replace("\x7fenc\x1f", "").replace("\t", "\\t")
    return (tag + " " if tag else "") + s


def load_merged(udb):
    """跨文件合并视图：.ldb 内取最新版本；后处理的文件覆盖先前的；WAL 删除生效。
    返回 {user_key: (value_or_None, 来源文件)}。供 _mine_userdb.py 复用。"""
    merged = {}
    for f in sorted(x for x in os.listdir(udb) if x.endswith((".log", ".ldb"))):
        path = os.path.join(udb, f)
        if f.endswith(".ldb"):
            for k, v in parse_ldb(path):
                merged[k] = (v, f)
        else:
            for op, k, v in parse_wal(path):
                if op == "DEL":
                    merged[k] = (None, f)
                else:
                    merged[k] = (v, f)
    return {k: t for k, t in merged.items() if t[0] is not None}


def main():
    args = [a for a in sys.argv[1:] if a != "--raw"]
    raw = "--raw" in sys.argv
    udb = args[0] if args else REAL_USERDB
    if not os.path.isdir(udb):
        print(f"[FATAL] userdb 目录不存在：{udb}")
        return 1
    print(f"# 解析 {udb} （只读）")
    files = sorted(f for f in os.listdir(udb) if f.endswith((".log", ".ldb")))
    if raw:
        for f in files:
            path = os.path.join(udb, f)
            recs = parse_ldb(path) if f.endswith(".ldb") else \
                [(k, v) for op, k, v in parse_wal(path) if op == "PUT"]
            print(f"-- {f} ({len(recs)} 条)")
            for k, v in recs:
                print(f"   [{f}] {_pretty(k)} => {_pretty(v)}")
    else:
        merged = load_merged(udb)
        print(f"# 合并视图（{len(merged)} 键，按 键\\t词 排序）")
        for k in sorted(merged):
            v, src = merged[k]
            print(f"   [{src}] {_pretty(k)} => {_pretty(v)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
