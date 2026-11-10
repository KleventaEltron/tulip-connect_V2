"""Compile the production observer with Harmony test doubles and run it."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
args = parser.parse_args()
tests = Path(__file__).resolve().parent
root = tests.parents[1]
compiler = [args.cc]
if Path(args.cc).stem.lower() == "zig":
    compiler.append("cc")

with tempfile.TemporaryDirectory(prefix="tulip-network-manager-") as temporary:
    executable = Path(temporary) / ("test.exe" if os.name == "nt" else "test")
    subprocess.run(compiler + [
        "-std=c99", "-Wall", "-Wextra", "-Werror",
        "-I" + str(tests / "stubs"), "-I" + str(root / "src/files"),
        str(root / "src/files/network_manager.c"),
        str(tests / "test_network_manager.c"), "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
