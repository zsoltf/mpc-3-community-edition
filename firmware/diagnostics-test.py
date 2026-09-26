#!/usr/bin/env python3
"""Linux fixture checks for the persistent USB debug-report collector.

The fixture supplies procfs, mountinfo, sysfs ancestry, DRM state, systemd
output and evdev records. It runs the production selection, bounded update,
identity-cache, report rendering and atomic-export code. Kernel mounts, real
ioctls, systemd and MPC process memory access are substituted.
"""
import os
import pathlib
import re
import shutil
import struct
import subprocess
import tempfile
import time

REPO = pathlib.Path(__file__).resolve().parents[1]
SOURCE = REPO / "firmware/usb-diagnostics.c"
MARKER = "MPCLEARN-DIAGNOSTICS"


def define(name, value):
    return f'-D{name}="{value}"'


def wait_text(path, predicate, seconds=6):
    end = time.monotonic() + seconds
    last = ""
    while time.monotonic() < end:
        try:
            last = path.read_text(errors="replace")
        except FileNotFoundError:
            pass
        if predicate(last):
            return last
        time.sleep(0.025)
    raise AssertionError(f"timed out waiting for {path}; last={last[-1000:]!r}")


def stop(process):
    process.terminate()
    assert process.wait(timeout=3) == 0


class Fixture:
    def __init__(self, base, report_cap=None):
        self.base = pathlib.Path(base)
        self.proc = self.base / "proc"
        self.media = self.base / "media"
        self.input = self.base / "input"
        self.sys_block = self.base / "sys/dev/block"
        self.drm = self.base / "drm"
        self.scratch = self.base / "run/current.txt"
        self.mpc = self.base / "MPC"
        self.observer = self.base / "command-observer.so"
        self.command = self.base / "command.state"
        self.mirror = self.base / "volume.state"
        self.settings = self.base / "MPC.settings"
        self.session = self.base / "session"
        self.systemctl = self.base / "systemctl"
        for directory in [self.proc / "self", self.media, self.input,
                          self.scratch.parent, self.session, self.sys_block,
                          self.drm / "1"]:
            directory.mkdir(parents=True, exist_ok=True)
        self.mpc.write_bytes((REPO / "build/v0_2_5/MPC").read_bytes())
        self.observer.write_bytes(
            (REPO / "artifacts/mcu-v0_2_5/command-observer.so").read_bytes())
        self.settings.write_bytes(b"fixture settings are not read")
        self.systemctl.write_text("""#!/bin/sh
case "$3" in
  mpclearn-provision.service) printf 'ActiveState=failed\\nSubState=failed\\nResult=exit-code\\nExecMainStatus=1\\n' ;;
  *) printf 'ActiveState=inactive\\nSubState=dead\\nResult=success\\nExecMainStatus=0\\n' ;;
esac
""")
        self.systemctl.chmod(0o755)
        (self.drm / "1/name").write_text("rockchip drm\n")
        (self.drm / "1/state").write_text("""plane[33]: plane-0
 crtc=crtc-0
 fb=50
 format=XR24
 crtc-pos=800x1280+0+0
 src-pos=800.000000x1280.000000+0.000000+0.000000
plane[35]: plane-1
 crtc=null
 fb=0
 format=AR24
 crtc-pos=64x64+0+0
 src-pos=64.000000x64.000000+0.000000+0.000000
crtc[37]: crtc-0
 enable=1
 active=1
connector[39]: HDMI-A-1
 crtc=crtc-0
""")
        self.binary = self.base / "collector"
        args = [
            "gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
            "-DDIAGNOSTICS_TEST", "-DSCAN_INTERVAL_MS=100",
            "-DPOLL_INTERVAL_MS=20", "-DUSB_WRITE_INTERVAL_MS=1000",
            "-DCACHE_INTERVAL_MS=500", "-DQUERY_TIMEOUT_MS=100",
            "-DHASH_BYTES_PER_TICK=134217728",
            "-DHASH_START_INTERVAL_MS=500",
            define("PROC_ROOT", self.proc), define("DEV_INPUT_ROOT", self.input),
            define("MEDIA_ROOT", self.media),
            define("SYS_DEV_BLOCK_ROOT", self.sys_block),
            define("SCRATCH_FILE", self.scratch),
            define("MPC_EXE_PATH", self.mpc),
            define("OBSERVER_PATH", self.observer),
            define("DEV_OBSERVER_PATH", self.base / "dev-observer.so"),
            define("COMMAND_CLIENT", "/bin/true"),
            define("COMMAND_STATE", self.command),
            define("MIRROR_STATE", self.mirror),
            define("SETTINGS_PATH", self.settings), define("SESSION_DIR", self.session),
            define("SYSTEMCTL_PATH", self.systemctl),
            define("DRM_DEBUG_ROOT", self.drm),
            define("PAYLOAD_DIR", REPO / "artifacts/mcu-v0_2_5"),
            str(SOURCE), "-o", str(self.binary),
        ]
        if report_cap:
            args.insert(7, f"-DREPORT_CAP={report_cap}")
        subprocess.run(args, check=True)
        self.block_transport(usb=True)
        self.mounts([])

    def block_transport(self, usb):
        st = self.media.stat()
        link = self.sys_block / f"{os.major(st.st_dev)}:{os.minor(st.st_dev)}"
        if link.exists() or link.is_symlink():
            link.unlink()
        ancestry = ("usb1/1-1/block/sda/sda1" if usb else
                    "platform/internal/block/mmcblk0/mmcblk0p1")
        target = self.base / "sys/devices" / ancestry
        target.mkdir(parents=True, exist_ok=True)
        link.symlink_to(target)

    def usb(self, name="USB", writable=True, mount_id=40):
        root = self.media / name
        (root / MARKER).mkdir(parents=True, exist_ok=True)
        return (root, writable, mount_id)

    def mounts(self, entries):
        lines = []
        for entry in entries:
            root, writable, mount_id = entry[:3]
            mount_root = entry[3] if len(entry) > 3 else "/"
            st = root.stat()
            opts = "rw,relatime" if writable else "ro,relatime"
            point = str(root).replace(" ", "\\040")
            lines.append(
                f"{mount_id} 1 {os.major(st.st_dev)}:{os.minor(st.st_dev)} "
                f"{mount_root} {point} {opts} - vfat /dev/sda1 {opts}\n")
        (self.proc / "self/mountinfo").write_text("".join(lines))

    def process(self, tick, observer=True, state=False, cursor_enabled=0, x=0, y=0):
        p = self.proc / "101"
        p.mkdir(exist_ok=True)
        exe = p / "exe"
        if exe.exists() or exe.is_symlink():
            exe.unlink()
        exe.symlink_to(self.mpc)
        fields = ["S"] + ["0"] * 49
        fields[19] = str(tick)
        (p / "stat").write_text("101 (MPC worker name) " + " ".join(fields) + "\n")
        lines = []
        paths = [(0x10000, self.mpc)]
        if observer:
            paths.append((0x8000000, self.observer))
        for at, path in paths:
            st = path.stat()
            lines.append(
                f"{at:08x}-{at+4096:08x} r-xp 00000000 "
                f"{os.major(st.st_dev):02x}:{os.minor(st.st_dev):02x} "
                f"{st.st_ino} {path}\n")
        (p / "maps").write_text("".join(lines))
        with (p / "mem").open("wb") as target:
            target.seek(0x1000 + 0x0C)
            target.write(struct.pack("I", 0))
            target.seek(0x1000 + 0x20)
            target.write(struct.pack("I", 0))
            target.seek(0x1000 + 0x24)
            target.write(struct.pack("III", 0x3000, 0x3004, 0x3010))
            target.seek(0x1000 + 0x30)
            target.write(struct.pack("ii", x, y))
            target.seek(0x1000 + 0x38)
            target.write(bytes([cursor_enabled]))
            target.seek(0x10000 + 0x06B220D4)
            target.write(struct.pack("I", 0x1000))
        if state:
            self.command.write_bytes(b"")
            self.mirror.write_bytes(b"")

    def replace_executable(self):
        replacement = self.base / "MPC.changed"
        shutil.copyfile(self.mpc, replacement)
        with replacement.open("r+b") as target:
            target.seek(0)
            first = target.read(1)
            target.seek(0)
            target.write(bytes([first[0] ^ 0x01]))
        replacement.replace(self.mpc)

    def events(self, values):
        replacement = self.input / "event-replacement"
        replacement.write_bytes(b"".join(
            struct.pack("llHHi", 0, 0, kind, code, value)
            for kind, code, value in values))
        replacement.replace(self.input / "event-fixture")

    def start(self, **extra):
        env = os.environ.copy()
        env.update(extra)
        return subprocess.Popen([self.binary], env=env)


def base_report_checks(text):
    assert "MPCLEARN USB debug report\nformat=2" in text
    assert "result=current" in text
    assert "collector_running_at_write=yes" in text
    assert "collection_finished" not in text
    assert "capture_limit_seconds" not in text
    assert "release_payload=verified" in text


def run_checks():
    # Prompt first whole report, cached status/DRM, stale replacement and idle stability.
    with tempfile.TemporaryDirectory(prefix="mpclearn-current-") as td:
        f = Fixture(td)
        usb = f.usb()
        report = usb[0] / MARKER / "report.txt"
        report.write_text("old=yes\nresult=complete\n")
        stale_tmp = usb[0] / MARKER / ".mpclearn-report.tmp"
        stale_tmp.write_text("old partial")
        f.mounts([usb])
        started = time.monotonic()
        p = f.start()
        text = wait_text(report, lambda s: "result=current" in s)
        assert time.monotonic() - started < 2
        base_report_checks(text)
        assert "old=yes" not in text and not stale_tmp.exists()
        f.settings.unlink()
        text = wait_text(report, lambda s: "status_cache=available" in s and
                         "drm_cache=available" in s and "settings=absent" in s)
        assert "unit name=mpclearn-provision.service active=failed" in text
        assert "drm_plane card=1 id=35 name=plane-1" in text
        assert "crtc=null format=AR24 crtc_size=64x64" in text
        time.sleep(0.7)  # allow an unchanged cache refresh before measuring idle
        idle = report.stat()
        time.sleep(1.5)
        idle_after = report.stat()
        assert (idle.st_ino, idle.st_mtime_ns) == (idle_after.st_ino, idle_after.st_mtime_ns)
        assert report.stat().st_size <= 256 * 1024
        stop(p)

    # Ambiguity before admission may resolve; invalidation after pinning never re-admits.
    with tempfile.TemporaryDirectory(prefix="mpclearn-destination-") as td:
        f = Fixture(td)
        first, second = f.usb("USB1", mount_id=40), f.usb("USB2", mount_id=41)
        first_report = first[0] / MARKER / "report.txt"
        second_report = second[0] / MARKER / "report.txt"
        f.mounts([first, second])
        p = f.start()
        wait_text(f.scratch, lambda s: "ambiguity_seen=yes" in s)
        assert not first_report.exists() and not second_report.exists()
        f.mounts([second])
        wait_text(second_report, lambda s: "result=current" in s)
        pinned = second_report.read_bytes()
        admitted = second[0] / (MARKER + "-old")
        (second[0] / MARKER).rename(admitted)
        (second[0] / MARKER).mkdir()
        f.mounts([second])
        wait_text(f.scratch, lambda s: "invalidated=yes" in s)
        f.mounts([first])
        f.settings.unlink()
        time.sleep(1.3)
        assert not first_report.exists()
        assert (admitted / "report.txt").read_bytes() == pinned
        stop(p)

    # Atomic failure stages leave a whole prior or whole replacement report.
    for stage in ("write", "rename", "dirsync"):
        with tempfile.TemporaryDirectory(prefix=f"mpclearn-fail-{stage}-") as td:
            f = Fixture(td)
            usb = f.usb()
            report = usb[0] / MARKER / "report.txt"
            fail = f.base / "fail-stage"
            f.mounts([usb])
            p = f.start(MPCLEARN_TEST_FAIL_FILE=str(fail))
            previous = wait_text(report, lambda s: "result=current" in s).encode()
            fail.write_text(stage)
            f.settings.unlink()
            wait_text(f.scratch, lambda s: "invalidated=yes" in s)
            current = report.read_bytes()
            if stage in ("write", "rename"):
                assert current == previous
            else:
                base_report_checks(current.decode())
            assert len(current) <= 256 * 1024
            stop(p)

    # Process rollover, cached hashing, input latency/rate cap and changed executable.
    with tempfile.TemporaryDirectory(prefix="mpclearn-process-") as td:
        f = Fixture(td)
        usb = f.usb()
        report = usb[0] / MARKER / "report.txt"
        export_log, hash_log = f.base / "exports.log", f.base / "hashes.log"
        f.mounts([usb])
        f.process(100, observer=True, state=True, cursor_enabled=0)
        p = f.start(MPCLEARN_TEST_EVDEV="1",
                    MPCLEARN_TEST_EXPORT_LOG=str(export_log),
                    MPCLEARN_TEST_HASH_LOG=str(hash_log))
        text = wait_text(report, lambda s: "start_ticks=100" in s and
                         "exe_verification=exact" in s and "cursor_last" in s)
        assert hash_log.read_text().splitlines() == ["hash"]
        event_started = time.monotonic()
        f.events([(2, 0, 1), (2, 1, 1), (1, 272, 1), (1, 272, 0), (2, 8, 1)])
        text = wait_text(report, lambda s: re.search(r"motion=2 .*left_down=1 left_up=1 wheel=1", s))
        assert time.monotonic() - event_started < 2
        for tick in range(200, 1200, 100):
            f.process(tick, observer=True, state=True,
                      cursor_enabled=1 if tick == 1100 else 0,
                      x=12 if tick == 1100 else 0, y=34 if tick == 1100 else 0)
            time.sleep(0.13)
        text = wait_text(report, lambda s: "current_start_ticks=1100" in s and
                         "cursor_last" in s and "x=12 y=34" in s)
        assert "mpc_epochs=8 total_epochs=11 dropped_epochs=3 epochs_truncated=yes" in text
        assert hash_log.read_text().splitlines() == ["hash"]
        for _ in range(25):
            f.events([(2, 0, 1), (2, 1, 1)])
            time.sleep(0.1)
        text = wait_text(report, lambda s: re.search(r"motion=[1-9][0-9]", s))
        writes = [int(line) for line in export_log.read_text().splitlines()]
        assert all(b - a >= 900 for a, b in zip(writes, writes[1:])), writes
        if os.environ.get("MPCLEARN_KEEP_REPORT"):
            shutil.copyfile(report, os.environ["MPCLEARN_KEEP_REPORT"])
        f.replace_executable()
        f.process(2000, observer=True, state=True)
        text = wait_text(report, lambda s: "current_start_ticks=2000" in s and
                         "exe_verification=mismatch" in s, seconds=8)
        assert len(hash_log.read_text().splitlines()) == 2
        assert "exact_build=no" in text
        stop(p)

    # Saturating counters do not wrap under additional events.
    with tempfile.TemporaryDirectory(prefix="mpclearn-saturate-") as td:
        f = Fixture(td)
        usb = f.usb()
        report = usb[0] / MARKER / "report.txt"
        f.mounts([usb])
        f.events([(2, 0, 1), (2, 0, 1), (2, 8, 1), (2, 8, 1),
                  (1, 272, 1), (1, 272, 1), (1, 272, 0), (1, 272, 0)])
        p = f.start(MPCLEARN_TEST_EVDEV="1", MPCLEARN_TEST_SATURATE="1")
        text = wait_text(report, lambda s: "motion=18446744073709551615" in s)
        assert "connections=4294967295" in text
        assert "left_down=18446744073709551615" in text
        assert "left_up=18446744073709551615" in text
        assert "wheel=18446744073709551615" in text
        stop(p)

    # Forced overflow retains a bounded, explicitly current parseable tail.
    with tempfile.TemporaryDirectory(prefix="mpclearn-cap-") as td:
        f = Fixture(td, report_cap=512)
        usb = f.usb()
        report = usb[0] / MARKER / "report.txt"
        f.mounts([usb])
        f.process(100, observer=True)
        p = f.start()
        text = wait_text(report, lambda s: "result=current" in s)
        assert report.stat().st_size <= 512
        assert "truncated=yes" in text and "collector_running_at_write=yes" in text
        stop(p)

    print("PASS: persistent current snapshots, prompt/delayed/pinned USB export, idle suppression, one-per-second coalescing, atomic failure stages, bounded output, saturating input counts, >8 current-preserving epochs, same-executable single hash, changed-executable re-verification, cursor parsing, cached systemd/command/DRM facts, and DRM trailing-section isolation")


if __name__ == "__main__":
    run_checks()
