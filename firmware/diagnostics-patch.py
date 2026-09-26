#!/usr/bin/env python3
"""Add the current USB diagnostics collector to one exact current no-SSH root."""
import hashlib
import json
import os
import pathlib
import subprocess
import sys

os.environ["E2FSPROGS_FAKE_TIME"] = "1783599420"
repo = pathlib.Path(__file__).resolve().parents[1]
if len(sys.argv) != 5:
    raise SystemExit("usage: diagnostics-patch.py ROOTFS COLLECTOR EXPECTATION BASE_SHA256")
rootfs, collector, expectation = map(pathlib.Path, sys.argv[1:4])
expected_base = sys.argv[4]
base_digest = hashlib.sha256(rootfs.read_bytes()).hexdigest()
if len(expected_base) != 64 or base_digest != expected_base:
    raise SystemExit("diagnostics must overlay the exact admitted no-SSH root")
if not collector.is_file() or collector.is_symlink():
    raise SystemExit("collector must be one regular ARMhf file")


def debug(command, write=True):
    args = ["debugfs"] + (["-w"] if write else []) + ["-R", command, str(rootfs)]
    result = subprocess.run(args, check=True, capture_output=True, text=True)
    if write and any(word in result.stderr.lower() for word in
                     ["not found", "error", "already exists", "usage:", "could not", "no free"]):
        raise RuntimeError(result.stderr)
    return result


def add_file(source, destination, mode):
    if "Inode:" in debug("stat " + destination, False).stdout:
        raise RuntimeError("unexpected existing path: " + destination)
    debug(f"write {source} {destination}")
    for field, value in [("mode", mode), ("uid", 0), ("gid", 0)]:
        debug(f"set_inode_field {destination} {field} {value}")
    return {"type": "file", "mode": mode,
            "sha256": hashlib.sha256(source.read_bytes()).hexdigest()}


def add_symlink(path, target):
    if "Inode:" in debug("stat " + path, False).stdout:
        raise RuntimeError("unexpected existing path: " + path)
    debug(f"symlink {path} {target}")
    return {"type": "symlink", "target": target}


added = {}
added["/usr/libexec/mpclearn/usb-diagnostics"] = add_file(
    collector, "/usr/libexec/mpclearn/usb-diagnostics", 0o100755)
added["/usr/lib/systemd/system/mpclearn-diagnostics.service"] = add_file(
    repo / "firmware/mpclearn-diagnostics.service",
    "/usr/lib/systemd/system/mpclearn-diagnostics.service", 0o100644)
link = "/usr/lib/systemd/system/multi-user.target.wants/mpclearn-diagnostics.service"
added[link] = add_symlink(link, "../mpclearn-diagnostics.service")
expectation.write_text(json.dumps(added, indent=2) + "\n")
