#!/usr/bin/env python3
"""Create the exact MPC 3.9.1 CE v0.2.6 executable from the pristine ELF."""
import hashlib
import json
import os
import pathlib
import sys

SPEC = json.loads((pathlib.Path(__file__).with_name("mpc-ce.json")).read_text())
EXPECTED_INPUT = SPEC["input_sha256"]
EXPECTED_OUTPUT = SPEC["output_sha256"]
EXPECTED_SIZE = SPEC["size"]
OFFSET = SPEC["offset"]
BEFORE = bytes.fromhex(SPEC["before_hex"])
AFTER = bytes.fromhex(SPEC["after_hex"])


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    if len(sys.argv) != 3:
        raise SystemExit("patch-mpc-ce.py PRISTINE_MPC OUTPUT_MPC")
    source, output = map(pathlib.Path, sys.argv[1:])
    if source.resolve() == output.resolve():
        raise SystemExit("output must not replace the pristine MPC executable")
    raw = source.read_bytes()
    if len(raw) != EXPECTED_SIZE or digest(raw) != EXPECTED_INPUT:
        raise SystemExit("wrong pristine MPC executable")
    if len(BEFORE) != len(AFTER) or raw[OFFSET:OFFSET + len(BEFORE)] != BEFORE:
        raise SystemExit("CE label slot changed")
    if raw.count(BEFORE) != 1:
        raise SystemExit("CE label slot is not unique")
    patched = raw[:OFFSET] + AFTER + raw[OFFSET + len(BEFORE):]
    differences = [i for i, (a, b) in enumerate(zip(raw, patched)) if a != b]
    allowed = [OFFSET + i for i, (a, b) in enumerate(zip(BEFORE, AFTER)) if a != b]
    if differences != allowed or len(patched) != len(raw):
        raise SystemExit("unexpected executable delta")
    if digest(patched) != EXPECTED_OUTPUT:
        raise SystemExit("patched executable identity changed")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".next")
    temporary.write_bytes(patched)
    os.chmod(temporary, source.stat().st_mode & 0o777)
    temporary.replace(output)
    print(f"PASS: {len(differences)} bytes changed only in 0x{OFFSET:08x}..0x{OFFSET + len(BEFORE):08x}")
    print(digest(patched))


if __name__ == "__main__":
    main()
