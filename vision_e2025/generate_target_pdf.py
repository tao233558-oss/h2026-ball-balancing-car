"""Generate a full-scale A4 target for camera-algorithm practice."""

from __future__ import annotations

from pathlib import Path

from reportlab.lib.colors import black, Color
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.pdfgen.canvas import Canvas

from .target_model import (
    BLACK_TAPE_WIDTH_MM,
    CENTER_DOT_DIAMETER_MM,
    CIRCLE_RADII_MM,
    RED_LINE_WIDTH_MM,
)


def generate(output_path: Path) -> Path:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    width, height = A4
    canvas = Canvas(str(output_path), pagesize=A4, pageCompression=1)
    canvas.setTitle("2025 E Problem A4 Vision Practice Target")
    canvas.setAuthor("Offline E2025 vision workspace")
    canvas.setSubject("Full-scale camera practice target; not UV-sensitive scoring paper")

    tape = BLACK_TAPE_WIDTH_MM * mm
    canvas.setFillColor(black)
    canvas.rect(0, 0, width, tape, stroke=0, fill=1)
    canvas.rect(0, height - tape, width, tape, stroke=0, fill=1)
    canvas.rect(0, 0, tape, height, stroke=0, fill=1)
    canvas.rect(width - tape, 0, tape, height, stroke=0, fill=1)

    red = Color(0.86, 0.0, 0.0)
    canvas.setStrokeColor(red)
    canvas.setFillColor(red)
    canvas.setLineWidth(RED_LINE_WIDTH_MM * mm)
    center_x = width / 2.0
    center_y = height / 2.0
    for radius_mm in CIRCLE_RADII_MM:
        radius = radius_mm * mm
        canvas.circle(center_x, center_y, radius, stroke=1, fill=0)
    canvas.circle(
        center_x,
        center_y,
        CENTER_DOT_DIAMETER_MM * mm / 2.0,
        stroke=0,
        fill=1,
    )
    canvas.showPage()
    canvas.save()
    return output_path


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    path = generate(root / "output" / "pdf" / "E2025_A4_vision_practice_target.pdf")
    print(path)


if __name__ == "__main__":
    main()
