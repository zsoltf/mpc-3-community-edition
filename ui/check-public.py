#!/usr/bin/env python3
"""Build the toolkit from a disposable clean copy of the public allow-list."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[1]
allow_list = repo / "release/public-files.json"
if not allow_list.is_file():
    subprocess.run([str(repo / "ui/build.sh")], cwd=repo, check=True)
    subprocess.run([str(repo / "ui/check.sh")], cwd=repo, check=True)
    print("PASS clean public checkout builds and exercises canonical UI toolkit")
    raise SystemExit(0)
files = json.loads(allow_list.read_text())
with tempfile.TemporaryDirectory(prefix="mpclearn-ui-public-") as directory:
    public = Path(directory)
    for target, source in files.items():
        source_path = repo / source
        if not source_path.is_file() or source_path.is_symlink():
            raise SystemExit(f"invalid allow-list source: {source}")
        output = public / target
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_path, output)
    required = {
        "ui/include/mpclearn-ui.h",
        "ui/private/mpc-3.9.1-profile.h",
        "ui/private/mpclearn-ui-private.h",
        "ui/mpclearn-ui.cc",
        "ui/examples/third-screen.h",
        "ui/examples/third-screen.cc",
        "ui/examples/slider-screen.h",
        "ui/examples/slider-screen.cc",
        "ui/examples/toolkit-example.h",
        "ui/examples/toolkit-example.cc",
        "ui/screens/global-midi-learn-groups.h",
        "ui/screens/global-midi-learn-groups.cc",
        "ui/screens/global-midi-learn-screen.h",
        "ui/screens/global-midi-learn-screen.cc",
        "ui/tests/component.cc",
        "ui/tests/public-surface.py",
        "ui/build.sh",
        "ui/check.sh",
    }
    missing = sorted(required - set(files))
    if missing:
        raise SystemExit("public UI closure missing: " + ", ".join(missing))
    subprocess.run([str(public / "ui/build.sh")], cwd=public, check=True)
    subprocess.run([str(public / "ui/check.sh")], cwd=public, check=True)
print("PASS clean disposable public allow-list copy builds and exercises canonical UI toolkit")
