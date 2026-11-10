"""Test actual logging socket functions in isolation from the full firmware.

Functions are extracted unchanged from logging.c; mocks follow the installed
Harmony close/options contract. This does not emulate TCP packets or hardware.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
tests = root / "tests/logging_socket"
source = (root / "src/files/logging.c").read_text()
functions = []
for signature in ["static void reportLoggingSocketFailure(", "bool startSocketConnection ( void )", "void closeSocket(void)"]:
    start = source.index(signature)
    end = source.index("\n}\n", start) + 3
    functions.append(source[start:end])
compiler = [args.cc]
if Path(args.cc).stem.lower() == "zig":
    compiler.append("cc")
with tempfile.TemporaryDirectory(prefix="tulip-logging-socket-") as temporary:
    temporary = Path(temporary)
    isolated = temporary / "logging_socket.c"
    isolated.write_text('#include "stubs.h"\n' + "\n".join(functions))
    executable = temporary / ("test.exe" if os.name == "nt" else "test")
    subprocess.run(compiler + ["-std=c99", "-Wall", "-Wextra", "-Werror", "-I" + str(tests),
        str(isolated), str(tests / "test_socket.c"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
