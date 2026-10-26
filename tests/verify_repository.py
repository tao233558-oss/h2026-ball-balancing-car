"""Verify archived file integrity and the selected STM32 baseline without hardware."""
import csv
import hashlib
import re
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "H2026_STM32F407" / "STM32F407_ProjectTemplate"


def main():
    with (ROOT / "docs/archive/source-manifest.csv").open(encoding="utf-8-sig", newline="") as handle:
        records = list(csv.DictReader(handle))
    errors = []
    tracked = set(subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode("utf-8").split("\0"))
    for entry in records:
        path = ROOT / entry["Path"]
        if entry["Path"] not in tracked:
            errors.append("untracked manifest entry: " + entry["Path"])
        elif not path.is_file():
            errors.append("missing: " + entry["Path"])
        elif hashlib.sha256(path.read_bytes()).hexdigest() != entry["SHA256"]:
            errors.append("hash: " + entry["Path"])
    if errors:
        raise AssertionError("\n".join(errors[:30]))
    baseline = PROJECT / "app/main.c"
    assert hashlib.sha256(baseline.read_bytes()).hexdigest() == "b50fe821d156d7876d14e95e16021fb0b0c713c5b812ceac4885d4219f3bf422"
    uvprojx = PROJECT / "project/MDK(V5)/Project.uvprojx"
    tree = ET.parse(uvprojx)
    for node in tree.findall(".//FilePath"):
        value = node.text or ""
        if "$" in value or re.match(r"^[A-Za-z]:", value):
            continue
        path = uvprojx.parent / Path(value.replace("\\", "/"))
        assert path.is_file(), "Keil source missing: " + value
    for match in re.finditer(r"\]\(([^)\n]+)\)", (ROOT / "README.md").read_text(encoding="utf-8")):
        target = match.group(1)
        if target.startswith(("http:", "https:", "#")) or "(" in target:
            continue
        assert (ROOT / target.split("#", 1)[0]).exists(), "README target missing: " + target
    print(f"PASS: {len(records)} file hashes, V17 baseline and Keil source references")


if __name__ == "__main__":
    main()
