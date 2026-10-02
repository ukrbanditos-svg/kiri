#!/usr/bin/env python3
import hashlib
import pathlib
import sys

EXPECTED_GIT_BLOB_SHA = "48060490e83d1524a8b0eea0cf1ded45c80c2a74"

def git_blob_sha(data: bytes) -> str:
    h = hashlib.sha1()
    h.update(f"blob {len(data)}\0".encode("ascii"))
    h.update(data)
    return h.hexdigest()

def main() -> int:
    if len(sys.argv) != 3:
        print("usage: generate_profiles.py <xp3filter.tjs> <output.h>", file=sys.stderr)
        return 2
    src = pathlib.Path(sys.argv[1])
    dst = pathlib.Path(sys.argv[2])
    data = src.read_bytes()
    sha = git_blob_sha(data)
    if sha != EXPECTED_GIT_BLOB_SHA:
        raise SystemExit(f"RuiTomo xp3filter hash mismatch: {sha} != {EXPECTED_GIT_BLOB_SHA}")
    text = data.decode("utf-8")
    if not text.isascii():
        raise SystemExit("xp3filter unexpectedly contains non-ASCII data")
    delim = "KIRIVN_RUITOMO_FVE_48060490"
    if f"){delim}\"" in text:
        raise SystemExit("raw string delimiter collision")
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(
        "#pragma once\n\n"
        "namespace KiriVNProfiles {\n"
        f"static const char RuiTomoFveXp3Filter[] = R\"{delim}({text}){delim}\";\n"
        "}\n",
        encoding="utf-8",
        newline="\n",
    )
    print(f"generated {dst} from verified blob {sha}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
