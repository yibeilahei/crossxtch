#!/usr/bin/env python3
"""Round-trip a synthetic XGF2 and check cmap / strides."""

from __future__ import annotations

import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / "tools/xgf2/gen_xgf2.py"


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "t.xgf2"
        subprocess.check_call(
            [
                sys.executable,
                str(GEN),
                "--synthetic",
                "--codepoints",
                "あいう漢字ABC",
                "--em",
                "16",
                "--ruby-em",
                "8",
                "-o",
                str(out),
            ]
        )
        data = out.read_bytes()
        magic, ver, flags, em, ruby_em, bpp, _r0, body_stride, ruby_stride, body_count, ruby_count, iv_count, _r1, iv_off, ruby_map_off, body_off, ruby_off = struct.unpack_from(
            "<IHHBBBBHHHHHHIIII", data, 0
        )
        assert magic == 0x32464758, hex(magic)
        assert ver == 1
        assert em == 16 and ruby_em == 8 and bpp == 2
        assert body_stride == ((16 + 3) // 4) * 16
        assert ruby_stride == ((8 + 3) // 4) * 8
        assert body_count >= 8
        assert iv_count >= 1
        assert iv_off == 64

        def lookup(cp: int) -> int | None:
            off = iv_off
            for _ in range(iv_count):
                first, last, gid = struct.unpack_from("<HHH", data, off)
                off += 6
                if first <= cp <= last:
                    return gid + (cp - first)
            return None

        assert lookup(ord("あ")) is not None
        assert lookup(ord("漢")) is not None
        assert lookup(ord("A")) is not None
        gid = lookup(ord("あ"))
        slot = data[body_off + gid * body_stride : body_off + (gid + 1) * body_stride]
        assert len(slot) == body_stride
        assert any(slot), "empty synthetic glyph"
        print(f"ok  glyphs={body_count} ruby={ruby_count} intervals={iv_count} size={len(data)}")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
