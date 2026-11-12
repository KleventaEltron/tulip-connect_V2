"""Compile the real engine/cJSON and validate requests against pinned OpenAPI."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import json
import yaml
from jsonschema import Draft7Validator, FormatChecker

parser = argparse.ArgumentParser()
parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
compiler = [args.cc]
if Path(args.cc).stem.lower() == "zig":
    compiler.append("cc")
with tempfile.TemporaryDirectory(prefix="tulip-s2-session-") as temporary:
    executable = Path(temporary) / ("test.exe" if os.name == "nt" else "test")
    subprocess.run(compiler + ["-std=c99", "-DTULIP_S2_COMM_TEST_ENABLE=0", "-Wall", "-Wextra", "-Werror", "-I" + str(root / "src"),
        "-I" + str(root / "src/files/s2"), str(root / "src/files/s2/s2_session.c"),
        str(root / "src/files/cJSON.c"), str(root / "tests/s2_session/test_session.c"),
        "-lm", "-o", str(executable)], check=True)
    result = subprocess.run([str(executable)], capture_output=True, text=True, check=True)
specdir = root / "tests/s2_session/spec"
spec = yaml.safe_load((specdir / "s2-connect-session-init.yml").read_text())
common = yaml.safe_load((specdir / "s2-connect-common.yml").read_text())
def dereference(value, document):
    # Resolve ONLY the two pinned local OpenAPI documents. No network retrieval.
    if isinstance(value, list):
        return [dereference(item, document) for item in value]
    if not isinstance(value, dict):
        return value
    if "$ref" in value:
        file, fragment = value["$ref"].split("#", 1)
        selected = {"": document, "s2-connect-common.yml": common,
                    "s2-connect-session-init.yml": spec}[file]
        target = selected
        for part in fragment.lstrip("/").split("/"):
            target = target[part.replace("~1", "/").replace("~0", "~")]
        return dereference(target, selected)
    return {key: dereference(item, document) for key, item in value.items()}

paths = spec["paths"]
schemas = {
    "REQUEST": paths["/initiateSession"]["post"]["requestBody"]["content"]["application/json"]["schema"],
    "INIT_RESPONSE": paths["/initiateSession"]["post"]["responses"]["200"]["content"]["application/json"]["schema"],
    "CONFIRM_RESPONSE": paths["/confirmAccessToken"]["post"]["responses"]["200"]["content"]["application/json"]["schema"],
}
checked = 0
for line in result.stdout.splitlines():
    kind, _, body = line.partition(" ")
    if kind in schemas:
        Draft7Validator(dereference(schemas[kind], spec), format_checker=FormatChecker()).validate(json.loads(body))
        checked += 1
    else:
        print(line)
assert checked == 3
print("Initiation request and server fixtures match the pinned official OpenAPI schemas.")
