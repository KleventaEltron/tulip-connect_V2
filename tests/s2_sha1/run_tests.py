"""Compare the production implementation with standard vectors and hashlib."""
import argparse
import hashlib
import os
from pathlib import Path
import random
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
compiler = [args.cc]
if Path(args.cc).stem.lower() == "zig":
    compiler.append("cc")
vectors = [
    (b"", "da39a3ee5e6b4b0d3255bfef95601890afd80709"),
    (b"abc", "a9993e364706816aba3e25717850c26c9cd0d89d"),
    (b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", "84983e441c3bd26ebaae4aa1f95129e5e54670f1"),
    (b"a" * 1000000, "34aa973cd4c4daa4f61eeb2bdbad27316534016f"),
    (b"dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11", "b37a4f2cc0624f1690f64606cf385945b2bec4ea"),
]
for n in [1, 2, 54, 55, 56, 57, 60, 63, 64, 65, 119, 120, 127, 128, 129, 511, 512, 513, 4096]:
    data = bytes(i % 256 for i in range(n))
    vectors.append((data, hashlib.sha1(data).hexdigest()))
rng = random.Random(42)  # Reproducible HOST test data only; no production randomness.
for _ in range(20):
    data = bytes(rng.randrange(256) for _ in range(rng.randrange(8192)))
    vectors.append((data, hashlib.sha1(data).hexdigest()))
with tempfile.TemporaryDirectory(prefix="tulip-s2-sha1-") as temporary:
    executable = Path(temporary) / ("test.exe" if os.name == "nt" else "test")
    subprocess.run(compiler + ["-std=c99", "-Wall", "-Wextra", "-Werror", "-I" + str(root / "src/files/s2"),
        str(root / "src/files/s2/s2_sha1.c"), str(root / "tests/s2_sha1/test_sha1.c"), "-o", str(executable)], check=True)
    for data, expected in vectors:
        result = subprocess.run([str(executable)], input=data, capture_output=True, check=True)
        assert result.stdout.decode().strip() == expected, (len(data), result.stdout, expected)
print(f"{len(vectors)} SHA-1 vectors, padding boundaries and independent hashlib comparisons passed.")
