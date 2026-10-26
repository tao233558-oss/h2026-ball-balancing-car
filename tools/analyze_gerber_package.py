from __future__ import annotations

import hashlib
import json
import math
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path


MIL_TO_MM = 0.0254
BOARD = {
    "xmin": -9.144,
    "xmax": 90.846,
    "ymin": -99.99,
    "ymax": 0.0,
}


def parse_rows(section: dict) -> list[dict]:
    fields = section["fields"]
    return [dict(zip(fields, row)) for row in section["rows"]]


def parse_excellon(path: Path) -> tuple[dict[str, float], list[tuple[str, float, float]]]:
    tools: dict[str, float] = {}
    hits: list[tuple[str, float, float]] = []
    current_tool: str | None = None
    for raw in path.read_text(encoding="ascii", errors="ignore").splitlines():
        line = raw.strip()
        match = re.fullmatch(r"T(\d+)C([0-9.]+)", line)
        if match:
            tools[match.group(1)] = float(match.group(2))
            continue
        match = re.fullmatch(r"T(\d+)", line)
        if match:
            current_tool = match.group(1)
            continue
        match = re.search(r"X(-?[0-9.]+)Y(-?[0-9.]+)", line)
        if match and current_tool:
            hits.append((current_tool, float(match.group(1)), float(match.group(2))))
    return tools, hits


def main(zip_path: Path, package_dir: Path) -> None:
    digest = hashlib.sha256(zip_path.read_bytes()).hexdigest()
    data = json.loads((package_dir / "FlyingProbeTesting.json").read_text(encoding="utf-8"))
    pins = parse_rows(data["pins"])
    actual = [pin for pin in pins if not str(pin["PIN_NAME"]).startswith("PAD")]

    # De-duplicate repeated pseudo/export rows by physical pin identity.
    unique: dict[tuple, dict] = {}
    for pin in actual:
        key = (
            pin["PIN_NAME"], pin["PIN_X"], pin["PIN_Y"], pin["LAYER"], pin["NET_NAME"],
            pin["PAD_SIZEX"], pin["PAD_SIZEY"], pin["HOLE_SIZE"], pin["HOLE_LEN"],
        )
        unique[key] = pin
    actual = list(unique.values())

    plated = [pin for pin in actual if max(float(pin["HOLE_SIZE"]), float(pin["HOLE_LEN"])) > 0]
    rings = []
    for pin in plated:
        copper_diameter = min(float(pin["PAD_SIZEX"]), float(pin["PAD_SIZEY"]))
        drill_diameter = max(float(pin["HOLE_SIZE"]), float(pin["HOLE_LEN"]))
        rings.append(((copper_diameter - drill_diameter) * MIL_TO_MM / 2, pin))
    rings.sort(key=lambda item: item[0])

    edge_clearances = []
    for pin in actual:
        x = float(pin["PIN_X"]) * MIL_TO_MM
        y = float(pin["PIN_Y"]) * MIL_TO_MM
        angle = int(float(pin["PAD_ANGLE"])) % 180
        sx = float(pin["PAD_SIZEX"]) * MIL_TO_MM
        sy = float(pin["PAD_SIZEY"]) * MIL_TO_MM
        if angle == 90:
            sx, sy = sy, sx
        clearance = min(
            x - sx / 2 - BOARD["xmin"],
            BOARD["xmax"] - x - sx / 2,
            y - sy / 2 - BOARD["ymin"],
            BOARD["ymax"] - y - sy / 2,
        )
        edge_clearances.append((clearance, pin))
    edge_clearances.sort(key=lambda item: item[0])

    net_endpoints: dict[str, list[str]] = defaultdict(list)
    for pin in actual:
        net_endpoints[str(pin["NET_NAME"])].append(str(pin["PIN_NAME"]))

    print(f"SHA256 {digest}")
    print(f"actual_unique_pins {len(actual)}")
    print(f"plated_actual_pins {len(plated)}")
    print("smallest_annular_rings_mm")
    for ring, pin in rings[:12]:
        print(f"  {ring:.4f} {pin['PIN_NAME']} net={pin['NET_NAME']} pad={pin['PAD_SIZEX']}x{pin['PAD_SIZEY']}mil hole={pin['HOLE_SIZE']}x{pin['HOLE_LEN']}mil")
    print("smallest_axis_aligned_pad_to_edge_mm")
    for clearance, pin in edge_clearances[:16]:
        print(f"  {clearance:.4f} {pin['PIN_NAME']} net={pin['NET_NAME']} xy=({pin['PIN_X']},{pin['PIN_Y']})mil")

    print("selected_net_endpoints")
    for net in ["D7", "PA03", "PA3", "PB15", "PA22", "PA10", "PA11", "PB00", "PB01", "PB02", "PB03"]:
        print(f"  {net}: {', '.join(net_endpoints.get(net, [])) or '<absent>'}")

    print("single_endpoint_nets")
    for net, endpoints in sorted(net_endpoints.items()):
        if len(endpoints) == 1:
            print(f"  {net}: {endpoints[0]}")

    for filename in ["Drill_PTH_Through.DRL", "Drill_PTH_Through_Via.DRL", "Drill_NPTH_Through.DRL"]:
        tools, hits = parse_excellon(package_dir / filename)
        counts = Counter(tool for tool, _, _ in hits)
        summary = ", ".join(f"T{tool}={tools.get(tool, math.nan):g}mm x{count}" for tool, count in sorted(counts.items()))
        print(f"{filename}: total={len(hits)} {summary}")


if __name__ == "__main__":
    main(Path(sys.argv[1]), Path(sys.argv[2]))
