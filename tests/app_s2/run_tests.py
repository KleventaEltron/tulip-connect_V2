"""Build and run the actual S2 application coordinator with callback doubles."""
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
with tempfile.TemporaryDirectory(prefix="tulip-app-s2-") as temporary:
    executable = Path(temporary) / ("test.exe" if os.name == "nt" else "test")
    subprocess.run(compiler + ["-std=c99", "-Wall", "-Wextra", "-Werror",
        "-I" + str(root / "tests/app_s2/stubs"), "-I" + str(root / "src"),
        str(root / "src/app_s2_tasks.c"), str(root / "tests/app_s2/test_app.c"),
        "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    subprocess.run([str(executable), "registration-failure"], check=True)
