"""Host-build the actual MCU-independent parser and controller sources."""
import shutil
import subprocess
import tempfile
import os
import shlex
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "H2026_STM32F407/STM32F407_ProjectTemplate"


def main():
    cc = os.environ.get("CC") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        raise SystemExit("gcc or clang is required for host syntax checks")
    # speed_pi.h depends on STM32 device headers; validate it in the full Keil build.
    sources = [PROJECT / "control/ball_balance.c", PROJECT / "control/ball_beam_controller.c"]
    sources += sorted((PROJECT / "protocol").rglob("*.c"))
    with tempfile.TemporaryDirectory() as build:
        for source in sources:
            command = [cc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-c", str(source), "-o", str(Path(build) / (source.stem + ".o"))]
            command += ["-I", str(PROJECT / "control"), "-I", str(PROJECT / "protocol")]
            command += shlex.split(os.environ.get("CFLAGS", ""))
            subprocess.run(command, check=True)
    print(f"PASS: host-compiled {len(sources)} actual controller/parser source files")


if __name__ == "__main__":
    main()
