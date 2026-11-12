"""Test actual HTTPS/session/cJSON with a controlled TLS byte stream."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
from email.parser import BytesParser

parser = argparse.ArgumentParser()
parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
compiler = [args.cc]
if Path(args.cc).stem.lower() == "zig":
    compiler.append("cc")
with tempfile.TemporaryDirectory(prefix="tulip-s2-https-") as temporary:
    executable = Path(temporary) / ("test.exe" if os.name == "nt" else "test")
    subprocess.run(compiler + ["-std=c99", "-DTULIP_S2_COMM_TEST_ENABLE=0", "-Wall", "-Wextra", "-Werror", "-I" + str(root / "src"),
        "-I" + str(root / "src/files/s2"), str(root / "src/files/s2/s2_https.c"),
        str(root / "src/files/s2/s2_session.c"), str(root / "src/files/cJSON.c"),
        str(root / "tests/s2_https/test_https.c"), "-lm", "-o", str(executable)], check=True)
    result = subprocess.run([str(executable)], capture_output=True, text=True)
    if result.returncode:
        print(result.stdout, result.stderr)
        result.check_returncode()
checked = 0
for line in result.stdout.splitlines():
    if line.startswith("REQUEST_HEX "):
        wire = bytes.fromhex(line.split(" ", 1)[1])
        headers, body = wire.split(b"\r\n\r\n", 1)
        request_line, headers = headers.split(b"\r\n", 1)
        assert request_line == b"POST /connection/v1/initiateSession HTTP/1.1"
        parsed = BytesParser().parsebytes(headers + b"\r\n\r\n")
        assert int(parsed["Content-Length"]) == len(body)
        assert parsed["Host"] == "cem.local:443"
        assert parsed["Authorization"].startswith("Bearer ")
        assert json.loads(body)["supportedS2MessageVersions"] == ["v1.0.0"]
        checked += 1
    else:
        print(line)
assert checked == 1
print("Generated HTTP request syntax/length independently checked with Python stdlib.")
