#!/usr/bin/env python3
"""Verify the diagnostics root differs from its exact base only by three additions."""
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile


def inventory(path):
    result = {}
    for line in pathlib.Path(path).read_text().splitlines():
        encoded, fields = line.split(" ", 1)
        name = bytes.fromhex(encoded).decode()
        if name in result:
            raise ValueError("duplicate inventory path")
        result[name] = fields
    return result


old, new = map(inventory, sys.argv[1:3])
expected = json.loads(pathlib.Path(sys.argv[3]).read_text())
rootfs = pathlib.Path(sys.argv[4])
required = {
    "/usr/libexec/mpclearn/usb-diagnostics",
    "/usr/lib/systemd/system/mpclearn-diagnostics.service",
    "/usr/lib/systemd/system/multi-user.target.wants/mpclearn-diagnostics.service",
}
assert set(expected) == required
assert new.keys() - old.keys() == required
assert not old.keys() - new.keys()
for name in old:
    assert old[name] == new[name], ("unexpected frozen-root metadata change", name)

with tempfile.TemporaryDirectory() as td:
    temp = pathlib.Path(td)
    for index, (name, spec) in enumerate(expected.items()):
        fields = dict(item.split(":", 1) for item in new[name].split())
        assert fields["uid"] == fields["gid"] == "0"
        assert fields["links"] == "1"
        if spec["type"] == "symlink":
            assert int(fields["mode"], 8) == 0o120777
            assert fields["sha256"] == hashlib.sha256(spec["target"].encode()).hexdigest()
        else:
            assert int(fields["mode"], 8) == spec["mode"]
            assert fields["sha256"] == spec["sha256"]
            output = temp / str(index)
            subprocess.run(["debugfs", "-R", f"dump {name} {output}", str(rootfs)],
                           check=True, capture_output=True)
            assert hashlib.sha256(output.read_bytes()).hexdigest() == spec["sha256"]

unit = pathlib.Path(sys.argv[5]).read_text()
assert "ExecStart=/usr/libexec/mpclearn/usb-diagnostics" in unit
assert "RuntimeDirectory=mpclearn-diagnostics" in unit
assert "TimeoutStartSec=" not in unit
assert "RuntimeMaxSec=" not in unit
assert "TimeoutStopSec=5" in unit
assert not any(token in unit for token in ["Requires=mpclearn", "PartOf=mpclearn", "After=mpclearn"])
print("PASS: exact current no-SSH root plus collector, independent unit and early multi-user Wants link")
