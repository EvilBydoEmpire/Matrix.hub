"""PlatformIO pre-build step: embed the current app from web/ and stamp the firmware with its commit."""
import logging
import pathlib
import subprocess
import sys

Import("env")  # noqa: F821 — provided by PlatformIO
ROOT = pathlib.Path(env["PROJECT_DIR"])  # noqa: F821
sys.path.insert(0, str(ROOT / "tools"))
import build_web  # noqa: E402

logging.basicConfig(level=logging.INFO, format="%(message)s")
build_web.write()
# The commit the firmware was built from, marked -dirty with uncommitted changes; About shows it.
version = subprocess.run(["git", "describe", "--always", "--dirty", "--abbrev=7"], cwd=ROOT,
                         capture_output=True, text=True).stdout.strip()
if version:
    env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])  # noqa: F821
