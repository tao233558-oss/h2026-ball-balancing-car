from __future__ import annotations

import math
import zipfile
from pathlib import Path

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_ALIGN_VERTICAL, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK, WD_LINE_SPACING
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor
from PIL import Image, ImageDraw, ImageFont
from pypdf import PdfReader


ROOT = Path(r"E:\咸鱼24电赛h\自动行驶小车_代码工程\电赛小车终极版（最终版)")
WORK = ROOT / "tmp" / "report_20260801"
ASSET = WORK / "assets"
OUT = ROOT / "交付"
OLD_PDF = Path(r"E:\车载平衡滚球运动控制系统_设计报告.pdf")
OFFICIAL_COVER = Path(r"C:\Users\pc\Downloads\设计报告封面.docx")
FINAL_WORK = ROOT / "tmp" / "report_final_364012047"
DOCX_OUT = FINAL_WORK / "364012047_s_generated.docx"

ASSET.mkdir(parents=True, exist_ok=True)
OUT.mkdir(parents=True, exist_ok=True)
FINAL_WORK.mkdir(parents=True, exist_ok=True)


# ----- Visual system -----
NAVY = "1F3A5F"
BLUE = "2B7188"
LIGHT = "EAF1F5"
MID = "B8C7D1"
DARK = "202A33"
ORANGE = "D9853B"
RED = "C00000"
GRAY = "65727E"
WHITE = "FFFFFF"

FONT_SONG = r"C:\Windows\Fonts\simsun.ttc"
FONT_HEI = r"C:\Windows\Fonts\simhei.ttf"
FONT_YAHEI = r"C:\Windows\Fonts\msyh.ttc"
FONT_YAHEI_BOLD = r"C:\Windows\Fonts\msyhbd.ttc"


def pil_font(size: int, bold: bool = False):
    return ImageFont.truetype(FONT_YAHEI_BOLD if bold else FONT_YAHEI, size)


def rgb(hex_color: str):
    return tuple(int(hex_color[i:i + 2], 16) for i in (0, 2, 4))


def draw_center(draw: ImageDraw.ImageDraw, xy, text: str, font, fill, anchor="mm"):
    draw.text(xy, text, font=font, fill=fill, anchor=anchor)


def rounded_box(draw, box, text, fill, outline=NAVY, font=None, text_fill=DARK,
                radius=22, width=3):
    draw.rounded_rectangle(box, radius=radius, fill=rgb(fill), outline=rgb(outline), width=width)
    x1, y1, x2, y2 = box
    draw.multiline_text(((x1 + x2) / 2, (y1 + y2) / 2), text,
                        font=font or pil_font(34), fill=rgb(text_fill),
                        anchor="mm", align="center", spacing=8)


def arrow(draw, p1, p2, color=BLUE, width=6, head=18):
    draw.line([p1, p2], fill=rgb(color), width=width)
    ang = math.atan2(p2[1] - p1[1], p2[0] - p1[0])
    a1 = ang + math.pi * 0.82
    a2 = ang - math.pi * 0.82
    pts = [p2,
           (p2[0] + head * math.cos(a1), p2[1] + head * math.sin(a1)),
           (p2[0] + head * math.cos(a2), p2[1] + head * math.sin(a2))]
    draw.polygon(pts, fill=rgb(color))


def save_diagram(img: Image.Image, name: str) -> Path:
    path = ASSET / name
    img.save(path, dpi=(220, 220))
    return path


def build_assets():
    # Reuse the exact mark embedded in the prescribed cover template.
    logo_path = ASSET / "competition_logo.png"
    with zipfile.ZipFile(OFFICIAL_COVER) as package:
        logo_path.write_bytes(package.read("word/media/image1.png"))

    # Overall architecture.
    img = Image.new("RGB", (1800, 760), "white")
    d = ImageDraw.Draw(img)
    f = pil_font(38)
    fb = pil_font(40, True)
    fs = pil_font(29)
    rounded_box(d, (700, 265, 1100, 515), "STM32F407VET6\n共用硬件平台\n分阶段固件验证", LIGHT, NAVY, fb)
    rounded_box(d, (80, 75, 470, 235), "K230视觉\n钢球中心像素 x", "F4F7F9", BLUE, f)
    rounded_box(d, (80, 300, 470, 460), "4路红外循迹\nS1–S4", "F4F7F9", BLUE, f)
    rounded_box(d, (80, 535, 470, 695), "SW1–SW4 + OLED\n标定、调试、急停", "F4F7F9", BLUE, f)
    rounded_box(d, (1330, 75, 1720, 235), "TB6612 + 编码器\n左右轮闭环", "F4F7F9", BLUE, f)
    rounded_box(d, (1330, 300, 1720, 460), "STEP/DIR驱动\n单步进电机齿条", "F4F7F9", BLUE, f)
    rounded_box(d, (1330, 535, 1720, 695), "失效保护\n300 ms收帧超时\n开环命令计数保护", "F4F7F9", ORANGE, f)
    arrow(d, (470, 155), (700, 330))
    arrow(d, (470, 380), (700, 390))
    arrow(d, (470, 615), (700, 455))
    arrow(d, (1100, 330), (1330, 155))
    arrow(d, (1100, 390), (1330, 380))
    arrow(d, (1100, 455), (1330, 615), ORANGE)
    arrow(d, (1330, 205), (1100, 295), GRAY, 4, 14)
    draw_center(d, (1220, 220), "速度反馈", fs, rgb(GRAY))
    save_diagram(img, "system_architecture.png")

    # Line following strategy.
    img = Image.new("RGB", (1800, 760), "white")
    d = ImageDraw.Draw(img)
    f = pil_font(36)
    fb = pil_font(39, True)
    fs = pil_font(28)
    d.rounded_rectangle((70, 70, 630, 690), radius=28, fill=rgb("F7F9FA"), outline=rgb(NAVY), width=3)
    draw_center(d, (350, 120), "车头传感器视图", fb, rgb(NAVY))
    d.line((120, 500, 580, 500), fill=rgb(DARK), width=45)
    d.arc((280, 320, 610, 650), 225, 325, fill=rgb(DARK), width=45)
    xs = [170, 290, 410, 530]
    for i, x in enumerate(xs, 1):
        d.rounded_rectangle((x - 43, 220, x + 43, 315), radius=12,
                            fill=rgb(LIGHT), outline=rgb(BLUE), width=3)
        draw_center(d, (x, 267), f"S{i}", f, rgb(NAVY))
    draw_center(d, (350, 375), "S1位于车身左侧", fs, rgb(GRAY))
    rounded_box(d, (760, 60, 1190, 190), "中心态\n0110 / 0100 / 0010", LIGHT, NAVY, f)
    rounded_box(d, (760, 255, 1190, 385), "边缘态\n1000 / 0001", "FFF5EA", ORANGE, f)
    rounded_box(d, (760, 450, 1190, 580), "丢线态\n0000：短时保向搜线", "F4F7F9", GRAY, f)
    rounded_box(d, (1340, 220, 1720, 420), "目标速度+转向量\n加减速斜坡\n左右轮PI", LIGHT, BLUE, f)
    arrow(d, (1190, 125), (1340, 270))
    arrow(d, (1190, 320), (1340, 320), ORANGE)
    arrow(d, (1190, 515), (1340, 370), GRAY)
    draw_center(d, (1450, 540), "弯道外侧不加速\n中心单点允许加速", fs, rgb(GRAY))
    save_diagram(img, "line_strategy.png")

    # Ball-beam mechanics.
    img = Image.new("RGB", (1800, 800), "white")
    d = ImageDraw.Draw(img)
    f = pil_font(38)
    fb = pil_font(42, True)
    fs = pil_font(30)
    pivot = (250, 575)
    end = (1510, 360)
    d.line((pivot, end), fill=rgb(NAVY), width=34)
    d.ellipse((210, 535, 290, 615), fill=rgb("FFFFFF"), outline=rgb(DARK), width=7)
    ball_x = 960
    slope = (end[1] - pivot[1]) / (end[0] - pivot[0])
    ball_y = int(pivot[1] + slope * (ball_x - pivot[0]) - 78)
    d.ellipse((ball_x - 58, ball_y - 58, ball_x + 58, ball_y + 58),
              fill=rgb("D9DEE2"), outline=rgb(DARK), width=5)
    draw_center(d, (ball_x, ball_y), "球", f, rgb(DARK))
    d.rounded_rectangle((1430, 360, 1645, 685), radius=20, fill=rgb(LIGHT), outline=rgb(BLUE), width=4)
    d.line((1535, 625, 1535, 420), fill=rgb(ORANGE), width=18)
    arrow(d, (1585, 610), (1585, 425), ORANGE, 7, 24)
    draw_center(d, (1535, 725), "单步进电机+齿条\n抬升车尾", f, rgb(NAVY))
    d.arc((260, 480, 520, 685), 270, 350, fill=rgb(ORANGE), width=6)
    draw_center(d, (480, 565), "α", fb, rgb(ORANGE))
    arrow(d, (960, ball_y + 90), (680, ball_y + 135), BLUE, 6, 22)
    draw_center(d, (800, ball_y + 185), "x<0：向车头方向", fs, rgb(BLUE))
    draw_center(d, (860, 105), "车头端限制平移并允许绕横轴转动；+STEP使车尾上升，钢球向负x加速", fb, rgb(NAVY))
    save_diagram(img, "ball_model.png")

    # Pixel-to-mm calibration.
    img = Image.new("RGB", (1800, 650), "white")
    d = ImageDraw.Draw(img)
    f = pil_font(38)
    fb = pil_font(42, True)
    fs = pil_font(31)
    d.line((180, 250, 1620, 250), fill=rgb(NAVY), width=8)
    points = [(260, "37 px", "-125 mm"), (900, "351 px", "0 mm"), (1525, "658 px", "+125 mm")]
    for x, ptxt, mtxt in points:
        d.line((x, 210, x, 290), fill=rgb(ORANGE if x == 900 else BLUE), width=7)
        draw_center(d, (x, 155), ptxt, fb, rgb(NAVY))
        draw_center(d, (x, 350), mtxt, f, rgb(ORANGE if x == 900 else BLUE))
    draw_center(d, (580, 470), "左段斜率 125/(351-37)=0.398 mm/px", fs, rgb(GRAY))
    draw_center(d, (1240, 470), "右段斜率 125/(658-351)=0.407 mm/px", fs, rgb(GRAY))
    draw_center(d, (900, 565), "分段线性标定消除左右视野尺度不一致，中点严格定义为0", fb, rgb(NAVY))
    save_diagram(img, "vision_map.png")

    # Balance state machine.
    img = Image.new("RGB", (1800, 780), "white")
    d = ImageDraw.Draw(img)
    f = pil_font(34)
    fb = pil_font(38, True)
    fs = pil_font(27)
    fl = pil_font(24)
    rounded_box(d, (70, 285, 360, 470), "LEVEL SETUP\nSW1/SW2手动调平", "F4F7F9", BLUE, f)
    rounded_box(d, (500, 285, 790, 470), "CENTER ACTIVE\nx*=0 mm", LIGHT, NAVY, fb)
    rounded_box(d, (930, 80, 1270, 250), "Q3 PHASE 0/1\n0 → +50 mm", LIGHT, BLUE, f)
    rounded_box(d, (930, 500, 1270, 690), "Q3 PHASE 2/3\n+50 → -50 mm", "FFF5EA", ORANGE, f)
    rounded_box(d, (1450, 80, 1740, 250), "VISION LOST\n>300 ms", "F4F7F9", GRAY, f)
    rounded_box(d, (1450, 500, 1740, 690), "SOFT LIMIT / ESTOP\n立即停止脉冲", "FFF1F1", RED, f)
    arrow(d, (360, 377), (500, 377))
    draw_center(d, (430, 245), "SW3建立水平参考", fl, rgb(GRAY))
    arrow(d, (790, 335), (930, 195))
    draw_center(d, (835, 125), "SW1启动题3", fl, rgb(GRAY))
    arrow(d, (1270, 250), (1270, 500), ORANGE)
    draw_center(d, (1390, 370), "单帧进入+50带", fs, rgb(ORANGE))
    arrow(d, (1270, 165), (1450, 165), GRAY)
    arrow(d, (1270, 595), (1450, 595), RED)
    draw_center(d, (1370, 550), "超限/急停", fs, rgb(RED))
    arrow(d, (930, 500), (790, 440), GRAY)
    draw_center(d, (760, 600), "3帧到位且|v|≤5", fl, rgb(GRAY))
    save_diagram(img, "balance_state.png")

    # Engineering close-loop / rollback figure.
    img = Image.new("RGB", (1800, 430), "white")
    d = ImageDraw.Draw(img)
    f = pil_font(34)
    boxes = [
        (60, 120, 350, 310, "观测\nOLED/编码器/像素"),
        (420, 120, 710, 310, "归因\n传感器·机械·时序"),
        (780, 120, 1070, 310, "单变量修改\n参数/逻辑"),
        (1140, 120, 1430, 310, "回归测试\n直线/弯道/丢线"),
        (1500, 120, 1740, 310, "固化版本\nV17可回退"),
    ]
    for i, b in enumerate(boxes):
        rounded_box(d, b[:4], b[4], LIGHT if i != 4 else "FFF5EA",
                    NAVY if i != 4 else ORANGE, f)
        if i < len(boxes) - 1:
            arrow(d, (b[2], 215), (boxes[i + 1][0], 215), BLUE, 5, 16)
    save_diagram(img, "engineering_loop.png")


def set_run_font(run, size=12, bold=False, color=DARK, east="宋体", latin="Times New Roman"):
    run.font.name = latin
    run.font.size = Pt(size)
    run.font.bold = bold
    run.font.color.rgb = RGBColor.from_string(color)
    rpr = run._element.get_or_add_rPr()
    rfonts = rpr.rFonts
    if rfonts is None:
        rfonts = OxmlElement("w:rFonts")
        rpr.insert(0, rfonts)
    rfonts.set(qn("w:eastAsia"), east)
    rfonts.set(qn("w:ascii"), latin)
    rfonts.set(qn("w:hAnsi"), latin)
    return run


def set_para_format(p, *, align=WD_ALIGN_PARAGRAPH.JUSTIFY, first=True,
                    line=22, before=0, after=0, keep_next=False):
    pf = p.paragraph_format
    pf.alignment = align
    pf.line_spacing_rule = WD_LINE_SPACING.EXACTLY
    pf.line_spacing = Pt(line)
    pf.space_before = Pt(before)
    pf.space_after = Pt(after)
    pf.first_line_indent = Cm(0.74) if first else None
    pf.keep_with_next = keep_next
    return p


def add_p(doc, text="", *, size=12, bold=False, color=DARK,
          align=WD_ALIGN_PARAGRAPH.JUSTIFY, first=True, line=22,
          before=0, after=0, east="宋体", latin="Times New Roman",
          keep_next=False):
    p = doc.add_paragraph()
    set_para_format(p, align=align, first=first, line=line,
                    before=before, after=after, keep_next=keep_next)
    if text:
        set_run_font(p.add_run(text), size, bold, color, east, latin)
    return p


def add_rich_p(doc, chunks, *, align=WD_ALIGN_PARAGRAPH.JUSTIFY, first=True,
               line=22, before=0, after=0):
    p = doc.add_paragraph()
    set_para_format(p, align=align, first=first, line=line, before=before, after=after)
    for chunk in chunks:
        if isinstance(chunk, str):
            text, bold, color, east, latin = chunk, False, DARK, "宋体", "Times New Roman"
        else:
            text = chunk[0]
            bold = chunk[1] if len(chunk) > 1 else False
            color = chunk[2] if len(chunk) > 2 else DARK
            east = chunk[3] if len(chunk) > 3 else "宋体"
            latin = chunk[4] if len(chunk) > 4 else "Times New Roman"
        set_run_font(p.add_run(text), 12, bold, color, east, latin)
    return p


def add_h1(doc, text):
    p = add_p(doc, text, size=16, bold=True, color=NAVY, align=WD_ALIGN_PARAGRAPH.LEFT,
              first=False, line=22, after=4, east="黑体", keep_next=True)
    return p


def add_h2(doc, text):
    p = add_p(doc, text, size=13, bold=True, color=DARK, align=WD_ALIGN_PARAGRAPH.LEFT,
              first=False, line=20, before=2, after=2, east="黑体", keep_next=True)
    return p


def add_formula(doc, text):
    return add_p(doc, text, size=11.5, color=NAVY, align=WD_ALIGN_PARAGRAPH.CENTER,
                 first=False, line=19, before=2, after=2, east="Cambria Math",
                 latin="Cambria Math")


def add_caption(doc, text):
    return add_p(doc, text, size=9, color=GRAY, align=WD_ALIGN_PARAGRAPH.CENTER,
                 first=False, line=14, before=1, after=3)


def set_cell_shading(cell, fill):
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = tc_pr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tc_pr.append(shd)
    shd.set(qn("w:fill"), fill)


def set_cell_margins(cell, top=55, start=75, bottom=55, end=75):
    tc = cell._tc
    tc_pr = tc.get_or_add_tcPr()
    tc_mar = tc_pr.first_child_found_in("w:tcMar")
    if tc_mar is None:
        tc_mar = OxmlElement("w:tcMar")
        tc_pr.append(tc_mar)
    for m, val in (("top", top), ("start", start), ("bottom", bottom), ("end", end)):
        node = tc_mar.find(qn(f"w:{m}"))
        if node is None:
            node = OxmlElement(f"w:{m}")
            tc_mar.append(node)
        node.set(qn("w:w"), str(val))
        node.set(qn("w:type"), "dxa")


def set_table_borders(table, color="8FA2AF", size="6", inside="4"):
    tbl_pr = table._tbl.tblPr
    borders = tbl_pr.first_child_found_in("w:tblBorders")
    if borders is None:
        borders = OxmlElement("w:tblBorders")
        tbl_pr.append(borders)
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        tag = borders.find(qn(f"w:{edge}"))
        if tag is None:
            tag = OxmlElement(f"w:{edge}")
            borders.append(tag)
        tag.set(qn("w:val"), "single")
        tag.set(qn("w:sz"), size if edge in ("top", "bottom") else inside)
        tag.set(qn("w:color"), color)


def set_repeat_table_header(row):
    tr_pr = row._tr.get_or_add_trPr()
    tbl_header = OxmlElement("w:tblHeader")
    tbl_header.set(qn("w:val"), "true")
    tr_pr.append(tbl_header)


def set_row_cant_split(row):
    tr_pr = row._tr.get_or_add_trPr()
    cant = OxmlElement("w:cantSplit")
    tr_pr.append(cant)


def add_table(doc, headers, rows, widths=None, font_size=9.5, alignments=None):
    table = doc.add_table(rows=1, cols=len(headers))
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = False
    table.style = "Table Grid"
    set_table_borders(table)
    hdr = table.rows[0]
    set_repeat_table_header(hdr)
    set_row_cant_split(hdr)
    for j, h in enumerate(headers):
        cell = hdr.cells[j]
        if widths:
            cell.width = Cm(widths[j])
        set_cell_shading(cell, LIGHT)
        set_cell_margins(cell)
        cell.vertical_alignment = WD_ALIGN_VERTICAL.CENTER
        p = cell.paragraphs[0]
        set_para_format(p, align=WD_ALIGN_PARAGRAPH.CENTER, first=False,
                        line=15, after=0)
        set_run_font(p.add_run(str(h)), font_size, True, NAVY, "黑体")
    for row_data in rows:
        row = table.add_row()
        set_row_cant_split(row)
        for j, value in enumerate(row_data):
            cell = row.cells[j]
            if widths:
                cell.width = Cm(widths[j])
            set_cell_margins(cell)
            cell.vertical_alignment = WD_ALIGN_VERTICAL.CENTER
            p = cell.paragraphs[0]
            align = alignments[j] if alignments else WD_ALIGN_PARAGRAPH.LEFT
            set_para_format(p, align=align, first=False, line=15, after=0)
            set_run_font(p.add_run(str(value)), font_size, False, DARK)
    return table


def add_picture(doc, path, width_cm, caption=None):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.first_line_indent = None
    p.paragraph_format.space_before = Pt(0)
    p.paragraph_format.space_after = Pt(0)
    p.paragraph_format.line_spacing_rule = WD_LINE_SPACING.SINGLE
    p.paragraph_format.line_spacing = 1
    p.add_run().add_picture(str(path), width=Cm(width_cm))
    if caption:
        add_caption(doc, caption)


def add_page_number(section, *, restart=None, linked=False):
    footer = section.footer
    footer.is_linked_to_previous = linked
    p = footer.paragraphs[0]
    p.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    p.paragraph_format.space_before = Pt(0)
    p.paragraph_format.space_after = Pt(0)
    run = p.add_run()
    set_run_font(run, 10.5, False, DARK)
    begin = OxmlElement("w:fldChar")
    begin.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = " PAGE "
    separate = OxmlElement("w:fldChar")
    separate.set(qn("w:fldCharType"), "separate")
    txt = OxmlElement("w:t")
    txt.text = "1"
    end = OxmlElement("w:fldChar")
    end.set(qn("w:fldCharType"), "end")
    run._r.extend([begin, instr, separate, txt, end])
    if restart is not None:
        sect_pr = section._sectPr
        for old in list(sect_pr.findall(qn("w:pgNumType"))):
            sect_pr.remove(old)
        pg_num = OxmlElement("w:pgNumType")
        pg_num.set(qn("w:start"), str(restart))
        sect_pr.append(pg_num)


def set_page_geometry(section, *, top, bottom, left, right, footer=0.8):
    section.page_width = Cm(21.0)
    section.page_height = Cm(29.7)
    section.top_margin = Cm(top)
    section.bottom_margin = Cm(bottom)
    section.left_margin = Cm(left)
    section.right_margin = Cm(right)
    section.header_distance = Cm(0.5)
    section.footer_distance = Cm(footer)
    section.gutter = Cm(0)


def configure_document(doc):
    section = doc.sections[0]
    set_page_geometry(section, top=1.0, bottom=2.0, left=3.17, right=3.17, footer=1.75)

    normal = doc.styles["Normal"]
    normal.font.name = "Times New Roman"
    normal.font.size = Pt(12)
    normal._element.rPr.rFonts.set(qn("w:eastAsia"), "宋体")
    pf = normal.paragraph_format
    pf.line_spacing_rule = WD_LINE_SPACING.EXACTLY
    pf.line_spacing = Pt(22)
    pf.space_before = Pt(0)
    pf.space_after = Pt(0)

    settings = doc.settings._element
    compat = settings.find(qn("w:compat"))
    if compat is None:
        compat = OxmlElement("w:compat")
        settings.append(compat)
    no_space = OxmlElement("w:doNotUseHTMLParagraphAutoSpacing")
    compat.append(no_space)


def page_break(doc):
    p = doc.add_paragraph()
    p.add_run().add_break(WD_BREAK.PAGE)


def add_toc_line(doc, title, page, level=0):
    p = doc.add_paragraph()
    set_para_format(p, align=WD_ALIGN_PARAGRAPH.LEFT, first=False,
                    line=24 if level == 0 else 21, after=2)
    p.paragraph_format.left_indent = Cm(0.65 * level)
    tabs = p.paragraph_format.tab_stops
    tabs.add_tab_stop(Cm(15.4), WD_ALIGN_PARAGRAPH.RIGHT, 1)
    run = p.add_run(f"{title}\t{page}")
    set_run_font(run, 12 if level == 0 else 11.5, level == 0,
                 NAVY if level == 0 else DARK,
                 "黑体" if level == 0 else "宋体")
    return p


def build_report():
    build_assets()
    doc = Document()
    configure_document(doc)

    # 1. Physical sealing sheet: only the team code is allowed here.
    add_p(doc, "364012047", size=16, bold=True,
          align=WD_ALIGN_PARAGRAPH.CENTER, first=False, line=22, after=0,
          east="宋体")

    # 2. Prescribed cover. Geometry, typography, logo size and vertical rhythm
    # are distilled from C:/Users/pc/Downloads/设计报告封面.docx.
    cover_section = doc.add_section(WD_SECTION.NEW_PAGE)
    set_page_geometry(cover_section, top=3.04, bottom=3.04,
                      left=3.17, right=3.17, footer=1.75)
    add_page_number(cover_section, restart=1, linked=False)
    add_p(doc, "", size=26, align=WD_ALIGN_PARAGRAPH.CENTER,
          first=False, line=22, after=9.2)
    add_p(doc, "", size=26, align=WD_ALIGN_PARAGRAPH.CENTER,
          first=False, line=22, after=9.2)
    add_p(doc, "", size=26, align=WD_ALIGN_PARAGRAPH.CENTER,
          first=False, line=22, after=9.2)
    add_p(doc, "", size=26, align=WD_ALIGN_PARAGRAPH.CENTER,
          first=False, line=22, after=14.6)
    add_p(doc, "2026“数智中原”河南省大学生电子设计大赛",
          size=20, bold=True, align=WD_ALIGN_PARAGRAPH.CENTER,
          first=False, line=22, after=24.2, east="方正小标宋简体")
    add_p(doc, "（H 题）", size=24, bold=True,
          align=WD_ALIGN_PARAGRAPH.CENTER, first=False, line=22,
          after=24.8, east="华文中宋")
    add_p(doc, "【本科组】", size=24, bold=True,
          align=WD_ALIGN_PARAGRAPH.CENTER, first=False, line=22,
          after=22.4, east="华文中宋")
    add_p(doc, "", size=22, align=WD_ALIGN_PARAGRAPH.CENTER,
          first=False, line=22, after=9.2)
    add_p(doc, "", size=22, align=WD_ALIGN_PARAGRAPH.CENTER,
          first=False, line=22, after=9.8)
    logo_p = doc.add_paragraph()
    logo_p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    logo_p.paragraph_format.first_line_indent = None
    logo_p.paragraph_format.space_before = Pt(0)
    logo_p.paragraph_format.space_after = Pt(10.8)
    logo_p.paragraph_format.line_spacing_rule = WD_LINE_SPACING.SINGLE
    logo_p.add_run().add_picture(str(ASSET / "competition_logo.png"), width=Cm(6.634))
    add_p(doc, "", size=15, first=False, line=22, after=9.2)
    add_p(doc, "", size=15, first=False, line=22, after=9.2)
    add_p(doc, "", size=15, first=False, line=22, after=9.2)
    add_p(doc, "", size=15, first=False, line=22, after=6.2)
    add_p(doc, "2026年8月1日", size=18, bold=True,
          align=WD_ALIGN_PARAGRAPH.CENTER, first=False, line=22,
          after=9.2, east="宋体")
    add_p(doc, "", size=18, first=False, line=22, after=0)

    # 3. Abstract + TOC + strict eight-page body. Numbering continues at 2.
    body_section = doc.add_section(WD_SECTION.NEW_PAGE)
    set_page_geometry(body_section, top=3.2, bottom=2.0,
                      left=2.2, right=2.2, footer=0.8)
    add_page_number(body_section, restart=2, linked=False)

    # 3. Abstract page.
    add_p(doc, "车载平衡滚球运动控制系统", size=18, bold=True,
          color=NAVY, align=WD_ALIGN_PARAGRAPH.CENTER, first=False,
          line=28, after=10, east="黑体")
    add_h1(doc, "摘  要")
    abstract = (
        "本系统基于STM32F407VET6完成四路红外循迹与单自由度滚球两个功能档的分阶段开发。"
        "循迹档采用四路红外离散状态、分段比例差速和双轮速度PI，实车可完成一圈，人工计时约40 s。"
        "滚球档由K230输出钢球中心坐标，经三点分段标定转换为毫米位置，以单步进齿条改变管道倾角，并设置300 ms收帧超时、急停和开环命令计数保护。"
        "测试已贯通视觉、串口、步进与保护链路；滚球仍受齿条回差、失步与视觉时延影响，阶段结果尚未形成重复性达标结论。"
    )
    assert len(abstract) <= 300, len(abstract)
    add_p(doc, abstract, size=12, line=22, after=6)
    add_rich_p(doc, [("关键词：", True, NAVY, "黑体"),
                     "差速循迹；车载滚球；视觉标定；步进电机；状态机"],
               align=WD_ALIGN_PARAGRAPH.LEFT, first=False, line=22, after=8)
    add_h2(doc, "系统特征摘要")
    add_table(doc,
              ["维度", "实现方法", "关键约束"],
              [
                  ("主控", "STM32F407VET6共用硬件平台", "循迹/滚球为分阶段固件"),
                  ("循迹", "S1–S4四路数字红外+双编码器", "弯道限速，丢线停车"),
                  ("滚球测量", "K230输出[x,y] ASCII，STM32仅取x", "9600 bit/s，中断接收"),
                  ("执行", "STEP/DIR单步进齿条调倾", "传动阈值与回差不对称"),
                  ("安全", "急停、收帧超时、命令计数保护", "无位置反馈，需人工建立零点"),
              ],
              widths=[2.6, 8.4, 5.2], font_size=10)
    add_caption(doc, "表1  系统技术路线概览")
    page_break(doc)

    # 4. Static TOC.
    add_p(doc, "目  录", size=18, bold=True, color=NAVY,
          align=WD_ALIGN_PARAGRAPH.CENTER, first=False, line=28, after=14, east="黑体")
    toc = [
        ("摘要", 2, 0),
        ("1  系统方案与总体设计", 4, 0),
        ("1.1  需求分解与总体架构", 4, 1),
        ("1.2  关键工程策略", 4, 1),
        ("2  红外循迹与差速运动控制", 5, 0),
        ("3  单自由度滚球机构与动力学", 6, 0),
        ("4  视觉测量、标定与串口链路", 7, 0),
        ("5  滚球闭环控制与题3状态机", 8, 0),
        ("6  硬件接口、软件结构与安全设计", 9, 0),
        ("7  系统测试与结果分析", 10, 0),
        ("8  误差来源、改进路径与结论", 11, 0),
        ("参考文献", 11, 0),
    ]
    for title, page, level in toc:
        add_toc_line(doc, title, page, level)
    page_break(doc)

    # 5. Body page 1.
    add_h1(doc, "1  系统方案与总体设计")
    add_h2(doc, "1.1  需求分解与总体架构")
    add_p(doc, "赛题同时要求小车沿地面黑线运动，以及车载装置对管内钢球进行定点和序列控制。本设计在同一STM32F407VET6硬件平台上建立循迹档和滚球档，两个功能已分阶段验证，当前可烧录V17为滚球专用入口，尚未合并为同一运行态。K230仅承担钢球视觉检测与图传，不直接驱动执行器。")
    add_picture(doc, ASSET / "system_architecture.png", 16.0, "图1  整车感知—决策—执行架构")
    add_h2(doc, "1.2  关键工程策略")
    add_table(doc, ["问题", "解决方法", "设计收益"], [
        ("直线与弯道对速度需求相反", "按红外模式和路程分段调度基速", "弯道不再因外侧单点压线而突然加速"),
        ("左右视野尺度不一致", "左—中—右三点分段线性标定", "保证中点0 mm，避免单比例引入偏置"),
        ("齿条执行有回差与起动阈值", "正负方向分别补偿+速度斜坡", "提高起动概率并减少过冲"),
        ("调参容易退化", "每次仅改一类参数，保留可回退版本", "快速恢复已知可用状态"),
    ], widths=[4.1, 7.4, 4.7], font_size=9.3)
    add_caption(doc, "表2  系统的约束驱动型设计")
    page_break(doc)

    # 6. Body page 2.
    add_h1(doc, "2  红外循迹与差速运动控制")
    add_p(doc, "四路红外模块从车身左到右依次为S1–S4，白纸输出0，黑线输出1。程序不将“中心”等同于固定的0110：实车直线时常仅S2或S3单点压线，因此0100、0010也划入可加速的中心态；1000和0001则明确判定为大偏差，优先降速与转向。")
    add_picture(doc, ASSET / "line_strategy.png", 16.0, "图2  四路红外模式分类与速度调度")
    add_formula(doc, "δ = sat[Kₛ·eₛₜₐₜₑ, ±δₘₐₓ]；  vₗ* = vᵇ + δ，  vᵣ* = vᵇ - δ")
    add_formula(doc, "vᵇ = f(4 bit红外模式，编码器累计量D，丢线持续时间)")
    add_p(doc, "实际固定路线版采用“离散模式+分段比例差速+边缘特判”，未对红外误差引入导数项。左右轮由增量编码器构成速度PI内环，前进脉冲均为正。路线层依据编码器累计量D在弯前提前减速，并采用2/4的加、减速步长保证连续；丢线时短时沿上一方向搜线，超时则停车。")
    add_table(doc, ["模式", "控制意图", "实现约束"], [
        ("0110/0100/0010", "中心保持，允许较高基速", "转向量限幅防抖"),
        ("1000/0001", "大偏差抢救，降速转向", "外侧单点绝不加速"),
        ("0000", "按最后方向低速搜线", "600 ms未恢复即停车"),
    ], widths=[4.2, 7.2, 4.8], font_size=9.6)
    add_caption(doc, "表3  四路模式到差速控制意图的映射")
    page_break(doc)

    # 7. Body page 3.
    add_h1(doc, "3  单自由度滚球机构与动力学")
    add_h2(doc, "3.1  机械结构")
    add_p(doc, "滚球管总长250 mm，中点O为坐标原点，向车尾为正。管道车头端在平移方向受约束，但可绕横向铰轴转动；车尾端由步进电机齿轮—齿条机构升降，从而改变纵向倾角。当前结构存在塑料齿条弹性、回差、导向卡滞和丢步风险，开环脉冲计数不能等同于真实位置。")
    add_picture(doc, ASSET / "ball_model.png", 16.0, "图3  单步进齿条滚球机构及坐标约定")
    add_h2(doc, "3.2  小角度动力学与标定")
    add_formula(doc, "ẍ = -(5/7)g sinα ≈ -(5/7)gα（理想纯滚动、小角度且车体静止）")
    add_p(doc, "定义车尾上升为α>0，则钢球向负x运动，故理想模型含负号。实测+80步对应-0.27°、-80步对应+0.35°，两方向斜率约296和229步/度，260步/度仅作名义平均值。在当前负载、预紧与装配下，观察到正负向命令起动阈值约+288/-253步，其中同时包含回差、弹性、卡滞及球起滚条件。")
    add_table(doc, ["标定项", "实测值", "控制用途"], [
        ("管长/坐标范围", "250 mm / -125∼+125 mm", "定义题目位置"),
        ("正/负向斜率", "296 / 229步/度", "名义平均约260步/度"),
        ("正/负命令起动阈值", "+288 / -253步", "当前装配下的综合阈值"),
        ("相对命令计数保护", "±420脉冲", "降低撞限风险，不替代物理限位"),
    ], widths=[5.0, 5.0, 6.2], font_size=9.5)
    add_caption(doc, "表4  滚球机构标定参数")
    page_break(doc)

    # 8. Body page 4.
    add_h1(doc, "4  视觉测量、标定与串口链路")
    add_h2(doc, "4.1  三点分段标定")
    add_p(doc, "摄像头固定在管道上方，K230识别钢球中心并给出像素坐标(x,y)。试验按键依次将球放在左端、中点和右端，每点连续取样后确认。本车标定值为37 px、351 px、658 px。由于镜头透视和安装偏心，左右像素尺度不完全相同，故以中点为分界做分段线性映射。")
    add_picture(doc, ASSET / "vision_map.png", 16.0, "图4  像素坐标到管道毫米坐标的三点标定")
    add_formula(doc, "xₘₘ={-125+(p-37)·125/(351-37), p≤351;  (p-351)·125/(658-351), p>351}")
    add_h2(doc, "4.2  最小数据串与异步接收")
    add_p(doc, "K230与STM32采用单向3.3 V TTL串口：K230 40Pin排针第11脚GPIO5/TX接STM32 PA3/USART2_RX，两板必须共地；K230由独立5 V/USB-C供电。通信参数为9600 bit/s、8数据位、无校验、1停止位。视觉端只发送ASCII数据串“[x,y]\\r\\n”，主控仅取x参与闭环，协议本身不含VALID或AGE字段。USART2在RXNE中断中逐字节入环形缓冲，主循环非阻塞解析；每收到一帧合法坐标即刷新本地时间戳。")
    add_table(doc, ["链路状态", "判定", "处理"], [
        ("正常", "收到合法坐标帧，本地帧龄≤300 ms", "更新位置、速度和外环目标"),
        ("帧间保持", "尚无新帧且未超时", "保持既有目标，不重复积分"),
        ("视觉失效", "连续300 ms未收到合法新帧", "停止STEP脉冲，进入VISION LOST"),
    ], widths=[4.0, 6.1, 6.1], font_size=9.4)
    add_caption(doc, "表5  视觉链路时效判定与安全处理")
    page_break(doc)

    # 9. Body page 5.
    add_h1(doc, "5  滚球闭环控制与题3状态机")
    add_h2(doc, "5.1  位置—速度控制律")
    add_p(doc, "钢球系统具有惯性和延迟，只用位置比例项会在目标附近往复穿越。因此以像素转换的位置x为状态，对相邻有效帧差分得到速度v，用位置—速度反馈产生执行器命令。当球近似静止且距目标较远时加入定向起动补偿；接近目标后由速度项提前制动。当前执行器位置P仅为已发送STEP脉冲的开环累计量，不代表齿条真实位移。")
    add_formula(doc, "e=x-x*；  s*=sat[Kₚ e+Kᵥ v+uₑ, ±380]；  ṡ=rate_limit(s*-s)")
    add_picture(doc, ASSET / "balance_state.png", 16.0, "图5  调平、中心保持、题3序列与故障状态机")
    add_h2(doc, "5.2  题3序列控制")
    add_p(doc, "完成水平参考并进入ACTIVE后，SW1启动题3序列：目标由0 mm切换至+50 mm；检测到一帧进入±10 mm到达带后切换为-50 mm；最终只有连续3帧位于±10 mm且|v|≤5 mm/s才判定完成。5 s为题目计时判据，7 s为程序安全中止阈值。STEP目标每10 ms作斜坡更新；该策略将中间换向与最终静止判定分开，避免穿越终点时误判完成。")
    add_table(doc, ["参数", "当前值", "意义"], [
        ("中心保持Kₚ/Kᵥ", "4 / 8", "低增益恢复与速度阻尼"),
        ("题3 Kₚ/Kᵥ", "8 / 6", "快速序列实验参数"),
        ("起动补偿/输出限幅", "330 / 380步", "题3克服死区并限制倾角"),
        ("收帧超时/安全中止", "300 ms / 7 s", "丢失视觉或超时即停机"),
        ("相对命令计数保护", "±420脉冲", "降低撞限风险，非真实限位"),
    ], widths=[5.0, 4.2, 6.2], font_size=9.3)
    add_caption(doc, "表6  V17滚球控制的阶段性实验参数")
    page_break(doc)

    # 10. Body page 6.
    add_h1(doc, "6  硬件接口、软件结构与安全设计")
    add_h2(doc, "6.1  关键接口分配")
    add_table(doc, ["模块", "信号与STM32引脚", "功能/备注"], [
        ("四路红外", "S1 PB1；S2 PB0；S3 PC5；S4 PC4", "白0黑1，S1在车身左侧"),
        ("TB6612左/右轮", "PE13/PE14 PWM；PD0–PD3方向；PD4 STBY", "VCC=3.3 V，VM接电机电源，共地"),
        ("左/右编码器", "PB4/PB5 TIM3；PB6/PB7 TIM4", "0–3.3 V；前进计数均增加"),
        ("DCC-100步进驱动", "STEP PA6；DIR PA7；SLP PA5；RST PA4", "控制共地；SLP/RST高；DCY悬空；禁止热插拔"),
        ("K230视觉", "GPIO5/TX → PA3/USART2_RX", "独立5 V/USB-C供电，3.3 V TTL，共地"),
        ("OLED / 按键", "PB10 SCL，PB11 SDA；PE7–PE10", "显示状态与人机操作"),
    ], widths=[3.7, 8.0, 4.5], font_size=8.8)
    add_caption(doc, "表7  单STM32F407主控版关键接线")
    add_h2(doc, "6.2  确定性时序与软件分层")
    add_p(doc, "软件按BSP、协议、控制、应用四层组织。受赛时进度约束，循迹与滚球复用同一硬件平台但采用两个分阶段固件入口：循迹版初始化红外、编码器、轮速PI与电机，V17滚球版初始化视觉串口、步进与状态机，二者尚未合并为同一运行态。正常闭环由1 ms时基调度，仅在步进唤醒等环节保留约3 ms短延时。")
    add_table(doc, ["任务", "周期/触发", "超时约束"], [
        ("滚球外环", "仅在收到新视觉帧时", "估计x/v并更新步进目标"),
        ("步进斜坡", "10 ms", "STEP由定时器产生，限加速度"),
        ("循迹与轮速", "20 ms", "模式解码、速度斜坡、PI输出"),
        ("OLED", "状态切换或空闲刷新", "强制刷新前先停STEP，不参与闭环"),
        ("USART2接收", "RXNE中断", "逐字节入队，主循环非阻塞解析"),
    ], widths=[4.0, 4.5, 7.7], font_size=9.2)
    add_caption(doc, "表8  周期任务与中断的时序分工")
    add_h2(doc, "6.3  故障安全")
    add_p(doc, "SW4为最高优先级急停；视觉超时、相对命令计数越界或未完成水平参考时，均禁止继续自动控制。发生故障后先将目标速度清零，再关闭脉冲输出。由于系统无原点开关和真实位移反馈，复位、堵转、失步或手动移动后必须重新调平建立零点；±420仅为命令计数保护，可靠保护仍需机械限位与回零传感器。")
    page_break(doc)

    # 11. Body page 7.
    add_h1(doc, "7  系统测试与结果分析")
    add_h2(doc, "7.1  分模块测试")
    add_table(doc, ["对象", "测试方法", "记录", "结论"], [
        ("电机/编码器", "架空定时正反转", "正转L4049/R4001；反转L-4088/R-3884", "方向与极性正确"),
        ("轮速均衡", "架空稳态观测", "正转PWM 249/249；反转247/242", "两轮基本一致"),
        ("四路红外", "白底及单通道压线", "0000；1000/0100/0010/0001", "通道顺序与逻辑正确"),
        ("视觉标定", "左端/中点/右端静态取样", "37/351/658 px → -125/0/+125 mm", "像素坐标与物理坐标一致"),
        ("视觉丢球", "取走钢球", "K230诊断VALID=0/AGE=999 ms", "STM32以300 ms无新帧触发保护"),
        ("步进方向", "SW1/SW2手动点动", "SW1车尾上升/POS增；SW2相反", "坐标符号统一"),
    ], widths=[2.8, 4.3, 6.1, 3.0], font_size=8.5)
    add_caption(doc, "表9  分模块实测记录")
    add_h2(doc, "7.2  循迹与滚球阶段结果")
    add_table(doc, ["版本/工况", "主要改动", "单次典型记录", "判定"], [
        ("循迹低速稳定版", "全程保守基速", "约50–51 s/圈", "可完成一圈，速度偏低"),
        ("路线分段版", "弯前刹车+直线加速", "累计量D≈40420；人工计时约40 s", "可完成一圈，未达20 s"),
        ("滚球V17序列", "0→+50→-50 mm，位置—速度反馈", "个别试验完成换向；终点曾约-10 mm", "重复性尚未验证"),
        ("后续增益试验", "加大起动和追踪力度", "出现+15∼+20 mm停留或过冲", "性能退化，已回退V17"),
    ], widths=[3.4, 5.0, 5.0, 2.8], font_size=8.5)
    add_caption(doc, "表10  系统联调阶段记录（为单次典型值，非统计平均）")
    add_p(doc, "测试表明，电气方向、基本测量链和故障分支均已贯通，但表中结果均为阶段性单次记录，不代表统计达标。循迹的主要矛盾是较高基速与弯道可观测时间的折中；滚球的主要限制是开环执行、回差/失步与视觉速度估计拖尾。后续应优先改善传感器前视距离、机构刚度和低速可控性，并开展不少于5次完整序列的重复试验。")
    page_break(doc)

    # 12. Body page 8.
    add_h1(doc, "8  误差来源、改进路径与结论")
    add_picture(doc, ASSET / "engineering_loop.png", 16.0, "图6  基于证据的单变量调试与版本回退流程")
    add_h2(doc, "8.1  误差预算与改进优先级")
    add_table(doc, ["误差源", "现象与证据", "影响", "改进方向"], [
        ("红外几何布置", "模块离地约9 mm，快速横移时状态闪烁", "弯道信息出现得过晚", "降低离地高度，增大车头前视距离"),
        ("底盘惯性", "直线稳定后进弯仍可向外滑移", "大偏差时转向量不足", "弯前减速，边缘态提前抢救"),
        ("开环执行/零位漂移", "P为脉冲累计；失步后仍可能P=T", "机构未到位却停止修正", "加原点/角度反馈，优化电流、电压和加速度"),
        ("齿条回差/卡滞", "正负起动值不等，小指令不动、大指令过冲", "目标附近残差或反复穿越", "合适细分、预紧导向并提高刚度"),
        ("视觉与车体扰动", "丢帧、时延；车辆加减速和俯仰未统计", "估速噪声和目标偏移", "时戳/外点剔除，记录均值、最大值和标准差"),
    ], widths=[3.4, 5.4, 3.8, 4.2], font_size=8.2)
    add_caption(doc, "表11  系统误差预算与改进优先级")
    add_h2(doc, "8.2  软件质量与可追溯性")
    add_p(doc, "最终回退至V17可复现的功能实验基线；Keil编译结果为0 error、0 warning，Code=19356 B，烧录经Erase、Programming和Verify成功。该版本保留水平参考、中心保持、题3序列、视觉超时、命令计数保护和急停，可作为后续回归起点；但其脉冲位置不是真实机构位置，不能据此宣称闭环精度稳定达标。")
    add_h2(doc, "8.3  结论")
    add_p(doc, "本设计在一套STM32F407硬件平台上分别贯通了红外—编码器—差速驱动链和视觉—串口—步进执行链，并以状态机管理到位判据与故障保护。循迹已实现约40 s完成一圈；滚球已完成坐标标定、水平参考及序列功能验证，但尚未形成重复性达标结论。下一步应采用“球位置外环—管道角度/齿条位移内环”，增加回零与真实位置反馈，再结合多次统计试验评价±5 cm目标精度。")
    add_h2(doc, "参考文献")
    refs = [
        "[1] 2026“数智中原”河南省大学生电子设计大赛H题：车载平衡滚球运动控制系统.",
        "[2] STMicroelectronics. RM0090 STM32F405/407 Reference Manual.",
        "[3] STMicroelectronics. STM32F407xx Datasheet.",
    ]
    for ref in refs:
        add_p(doc, ref, size=8.5, align=WD_ALIGN_PARAGRAPH.LEFT,
              first=False, line=12, after=0)

    # Core properties contain no identity information.
    doc.core_properties.title = "车载平衡滚球运动控制系统设计报告"
    doc.core_properties.subject = "2026“数智中原”河南省大学生电子设计大赛H题"
    doc.core_properties.author = ""
    doc.core_properties.last_modified_by = ""
    doc.save(DOCX_OUT)
    print(DOCX_OUT)


if __name__ == "__main__":
    build_report()
