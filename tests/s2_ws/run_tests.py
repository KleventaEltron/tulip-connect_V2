"""Build and run the actual portable WebSocket codec with callback doubles."""
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
with tempfile.TemporaryDirectory(prefix="tulip-s2-ws-") as temporary:
    executable = Path(temporary) / ("test.exe" if os.name == "nt" else "test")
    subprocess.run(compiler + ["-std=c99", "-DTULIP_S2_COMM_TEST_ENABLE=0", "-Wall", "-Wextra", "-Werror",
        "-I" + str(root / "src/files/s2"),
        str(root / "src/files/s2/s2_ws.c"),
        str(root / "src/files/s2/s2_sha1.c"),
        str(root / "tests/s2_ws/test_s2_ws.c"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
