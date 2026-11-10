"""Build and run the actual hardware entropy function with callback doubles."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
compiler = [args.cc]
if Path(args.cc).stem.lower() == "zig":
    compiler.append("cc")
with tempfile.TemporaryDirectory(prefix="tulip-crypto-random-") as temporary:
    executable = Path(temporary) / ("test.exe" if os.name == "nt" else "test")
    source = (root / "src/config/default/system/sys_random_h2_adapter.c").read_text()
    start = source.index("size_t SYS_RANDOM_CryptoBlockGet(")
    end = source.index("\n}\n", start) + 3
    adapter = Path(temporary) / "rng_adapter.c"
    adapter.write_text('#include "rng_adapter_stubs.h"\n' + source[start:end])
    subprocess.run(compiler + ["-std=c99", "-Wall", "-Wextra", "-Werror",
        "-I" + str(root / "tests/crypto_random/stubs"),
        "-I" + str(root / "tests/crypto_random"), "-I" + str(root / "src/files"),
        str(root / "src/files/crypto_random.c"), str(adapter),
        str(root / "tests/crypto_random/test_random.c"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
