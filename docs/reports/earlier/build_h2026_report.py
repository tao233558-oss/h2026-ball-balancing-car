from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK, WD_LINE_SPACING
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Inches, Pt, RGBColor


ROOT = Path(__file__).resolve().parent
FIG_DIR = ROOT / "figures"
OUT_DOCX = ROOT / "车载平衡滚球运动控制系统_设计报告.docx"
FIG_DIR.mkdir(parents=True, exist_ok=True)

FONT_CN = "C:/Windows/Fonts/simsun.ttc"
FONT_HEI = "C:/Windows/Fonts/simhei.ttf"

INK = "#1C2733"
BLUE = "#294E6B"
MID = "#6B7F8F"
PALE = "#EAF0F4"
LIGHT = "#F6F8FA"
ORANGE = "#B66836"


def pil_font(size, bold=False):
    return ImageFont.truetype(FONT_HEI if bold else FONT_CN, size)


def canvas():
    return Image.new("RGB", (1540, 600), "white")


def centered_multiline(draw, box, text, font, fill=INK, spacing=8):
    x0, y0, x1, y1 = box
    bbox = draw.multiline_textbbox((0, 0), text, font=font, spacing=spacing, align="center")
    w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
    draw.multiline_text(((x0 + x1 - w) / 2, (y0 + y1 - h) / 2),
                        text, font=font, fill=fill, spacing=spacing, align="center")


def rounded_box(draw, box, text, fc="white", ec=BLUE, fontsize=28, width=3):
    draw.rounded_rectangle(box, radius=18, fill=fc, outline=ec, width=width)
    centered_multiline(draw, box, text, pil_font(fontsize), fill=INK)


def arrow(draw, start, end, text=None, color=BLUE, text_offset=(0, -28)):
    draw.line([start, end], fill=color, width=4)
    ang = math.atan2(end[1] - start[1], end[0] - start[0])
    s = 14
    pts = [
        end,
        (end[0] - s * math.cos(ang - 0.55), end[1] - s * math.sin(ang - 0.55)),
        (end[0] - s * math.cos(ang + 0.55), end[1] - s * math.sin(ang + 0.55)),
    ]
    draw.polygon(pts, fill=color)
    if text:
        mx = (start[0] + end[0]) / 2 + text_offset[0]
        my = (start[1] + end[1]) / 2 + text_offset[1]
        bbox = draw.textbbox((0, 0), text, font=pil_font(22))
        draw.text((mx - (bbox[2] - bbox[0]) / 2, my), text, font=pil_font(22), fill=MID)


def save_img(img, name):
    path = FIG_DIR / name
    img.save(path, dpi=(220, 220))
    return path


def figure_system():
    img = canvas()
    d = ImageDraw.Draw(img)
    rounded_box(d, (30, 75, 295, 215), "四路红外巡线模块\n车轮编码器", fc=LIGHT, fontsize=25)
    rounded_box(d, (30, 330, 295, 470), "摄像头\n钢球图像", fc=LIGHT)
    rounded_box(d, (430, 60, 760, 230), "STM32F407\n运动控制与状态机", fc=PALE, width=5)
    rounded_box(d, (430, 315, 760, 485), "K230\n检测、标定与推流", fc=PALE, width=5)
    rounded_box(d, (910, 60, 1150, 165), "TB6612\n左右轮电机", fontsize=26)
    rounded_box(d, (910, 230, 1150, 335), "步进驱动器\n齿条升降", fontsize=26)
    rounded_box(d, (910, 395, 1150, 500), "2.4 GHz Wi-Fi\nRTSP图传", fontsize=25)
    rounded_box(d, (1250, 395, 1510, 500), "场外接收端\n显示与录像", fc=LIGHT, fontsize=25)
    arrow(d, (295, 145), (430, 145), "4位状态/速度")
    arrow(d, (295, 400), (430, 400), "图像")
    arrow(d, (760, 125), (910, 112), "PWM")
    arrow(d, (760, 175), (910, 280), "STEP/DIR")
    arrow(d, (595, 315), (595, 230), "x、置信度、状态", text_offset=(115, -4))
    arrow(d, (760, 400), (910, 448), "码流")
    arrow(d, (1150, 448), (1250, 448), "实时视频")
    footer = "电池供电经12 V、5 V与3.3 V分级稳压，各控制地单点汇接"
    bbox = d.textbbox((0, 0), footer, font=pil_font(23))
    d.text(((1540 - bbox[2]) / 2, 555), footer, font=pil_font(23), fill=MID)
    return save_img(img, "fig1_system.png")


def figure_line_control():
    img = Image.new("RGB", (1540, 470), "white")
    d = ImageDraw.Draw(img)
    rounded_box(d, (25, 30, 280, 145), "四路GPIO采样\n黑线=1，白底=0", fc=LIGHT, fontsize=25)
    rounded_box(d, (350, 30, 655, 145), "相邻“1”分组\n按上一位置择近", fc=PALE, fontsize=25)
    rounded_box(d, (725, 30, 1010, 145), "组内权值平均\n得到横向误差 e", fc=PALE, fontsize=25)
    rounded_box(d, (1080, 30, 1510, 145), "转向PD与分段降速\n左右轮独立速度PI", fc=LIGHT, fontsize=25)
    arrow(d, (280, 88), (350, 88))
    arrow(d, (655, 88), (725, 88))
    arrow(d, (1010, 88), (1080, 88))
    d.line([(125, 310), (1415, 310)], fill=INK, width=14)
    xs = [470, 670, 870, 1070]
    states = [0, 1, 1, 0]
    weights = [-30, -10, 10, 30]
    for i, (x, state, weight) in enumerate(zip(xs, states, weights), start=1):
        fill = BLUE if state else "#CED7DE"
        d.rounded_rectangle((x - 58, 230, x + 58, 350), radius=10, fill=fill, outline="white", width=3)
        label = f"S{i}  w={weight:+d}"
        bbox = d.textbbox((0, 0), label, font=pil_font(20))
        d.text((x - (bbox[2] - bbox[0]) / 2, 365), label, font=pil_font(20), fill=MID)
    arrow(d, (770, 230), (770, 175), "B=0110，e=0", color=ORANGE, text_offset=(0, -34))
    footer = "e=Σ(wi·bi)/Σbi；若出现多个不相邻有效组，只计算与上一周期位置最近的一组"
    bbox = d.textbbox((0, 0), footer, font=pil_font(22))
    d.text(((1540 - (bbox[2] - bbox[0])) / 2, 425), footer, font=pil_font(22), fill=INK)
    return save_img(img, "fig2_line_control.png")


def figure_ball_mechanism():
    img = canvas()
    d = ImageDraw.Draw(img)
    x1, y1 = 180, 360
    x2, y2 = 1250, 225
    d.line([(x1, y1), (x2, y2)], fill=INK, width=22)
    d.ellipse((700, 225, 825, 350), fill="#B9C4CC", outline=INK, width=3)
    d.line([(x1, 470), (x1, y1)], fill=INK, width=5)
    d.ellipse((x1 - 28, y1 - 28, x1 + 28, y1 + 28), fill="white", outline=BLUE, width=4)
    d.text((112, 485), "左端铰支", font=pil_font(26), fill=INK)
    d.line([(x2, 450), (x2, y2)], fill=BLUE, width=10)
    for yy in range(270, 430, 28):
        d.line([(x2, yy), (x2 + 45, yy)], fill=BLUE, width=3)
    d.ellipse((1320, 285, 1480, 445), fill=PALE, outline=BLUE, width=4)
    cx, cy = 1400, 365
    for ang in range(0, 360, 45):
        a = math.radians(ang)
        d.line([(cx + 55 * math.cos(a), cy + 55 * math.sin(a)),
                (cx + 86 * math.cos(a), cy + 86 * math.sin(a))], fill=BLUE, width=3)
    d.text((1275, 470), "步进电机+齿轮", font=pil_font(24), fill=INK)
    d.text((1110, 95), "齿条升降 h", font=pil_font(26), fill=BLUE)
    d.line([(x1, y1), (x2, y1)], fill=MID, width=2)
    d.arc((170, 315, 440, 450), start=180, end=198, fill=ORANGE, width=4)
    d.text((380, 330), "θ", font=pil_font(32), fill=ORANGE)
    arrow(d, (780, 175), (700, 190), "x", text_offset=(0, -35))
    formula = "a=(5/7)g sinθ≈(5/7)gθ，h=L sinθ"
    bbox = d.textbbox((0, 0), formula, font=pil_font(30))
    d.text(((1540 - bbox[2]) / 2, 25), formula, font=pil_font(30), fill=INK)
    footer = "右端采用带短行程补偿的升降接头，释放倾斜时的水平投影变化；机械限位限制最大倾角"
    bbox = d.textbbox((0, 0), footer, font=pil_font(22))
    d.text(((1540 - bbox[2]) / 2, 555), footer, font=pil_font(22), fill=MID)
    return save_img(img, "fig3_ball_mechanism.png")


def figure_state_machine():
    img = canvas()
    d = ImageDraw.Draw(img)
    boxes = [
        ((25, 55, 225, 165), "上电自检", LIGHT),
        ((335, 55, 535, 165), "调平标定", PALE),
        ((645, 55, 845, 165), "等待启动", PALE),
        ((955, 55, 1155, 165), "任务运行", "#E4EEF3"),
        ((1265, 55, 1515, 165), "到点停车", LIGHT),
    ]
    for box, text, fc in boxes:
        rounded_box(d, box, text, fc=fc, width=5 if text == "任务运行" else 3)
    for s, e in [((225, 110), (335, 110)), ((535, 110), (645, 110)),
                 ((845, 110), (955, 110)), ((1155, 110), (1265, 110))]:
        arrow(d, s, e)
    rounded_box(d, (255, 300, 510, 430), "视觉超时\n滚球回平", fc="#FFF5ED", ec=ORANGE, fontsize=26)
    rounded_box(d, (640, 300, 900, 430), "步进软限位\n停止执行", fc="#FFF5ED", ec=ORANGE, fontsize=26)
    rounded_box(d, (1030, 300, 1300, 430), "循迹丢线\n搜索后停车", fc="#FFF5ED", ec=ORANGE, fontsize=26)
    arrow(d, (1010, 165), (480, 300), ">200 ms", color=ORANGE, text_offset=(0, -5))
    arrow(d, (1050, 165), (770, 300), "越界", color=ORANGE, text_offset=(0, -5))
    arrow(d, (1100, 165), (1165, 300), ">0.6 s", color=ORANGE, text_offset=(0, -5))
    d.text((570, 462), "数据恢复并连续确认后返回运行", font=pil_font(23), fill=MID)
    footer = "10 ms滚球环、20 ms底盘环；通信、显示和录像解耦，辅助链路异常不直接驱动车轮"
    bbox = d.textbbox((0, 0), footer, font=pil_font(23))
    d.text(((1540 - bbox[2]) / 2, 548), footer, font=pil_font(23), fill=INK)
    return save_img(img, "fig4_state_machine.png")


OFFICIAL_LOGO = FIG_DIR / "official_competition_logo.png"
FIG1 = figure_system()
FIG2 = figure_line_control()
FIG3 = figure_ball_mechanism()
FIG4 = figure_state_machine()


def set_east_asia_font(run, east="宋体", latin="Times New Roman"):
    run.font.name = latin
    run._element.get_or_add_rPr()
    rfonts = run._element.rPr.rFonts
    if rfonts is None:
        rfonts = OxmlElement("w:rFonts")
        run._element.rPr.insert(0, rfonts)
    rfonts.set(qn("w:ascii"), latin)
    rfonts.set(qn("w:hAnsi"), latin)
    rfonts.set(qn("w:eastAsia"), east)


def set_run(run, size=12, bold=False, east="宋体", latin="Times New Roman",
            color=None, italic=False):
    set_east_asia_font(run, east=east, latin=latin)
    run.font.size = Pt(size)
    run.bold = bold
    run.italic = italic
    if color:
        run.font.color.rgb = RGBColor.from_string(color)
    return run


def set_repeat_table_header(row):
    tr_pr = row._tr.get_or_add_trPr()
    tbl_header = OxmlElement("w:tblHeader")
    tbl_header.set(qn("w:val"), "true")
    tr_pr.append(tbl_header)


def set_cell_shading(cell, fill):
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = tc_pr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tc_pr.append(shd)
    shd.set(qn("w:fill"), fill)


def set_cell_margins(cell, top=70, start=90, bottom=70, end=90):
    tc = cell._tc
    tc_pr = tc.get_or_add_tcPr()
    tc_mar = tc_pr.first_child_found_in("w:tcMar")
    if tc_mar is None:
        tc_mar = OxmlElement("w:tcMar")
        tc_pr.append(tc_mar)
    for m, v in (("top", top), ("start", start), ("bottom", bottom), ("end", end)):
        node = tc_mar.find(qn(f"w:{m}"))
        if node is None:
            node = OxmlElement(f"w:{m}")
            tc_mar.append(node)
        node.set(qn("w:w"), str(v))
        node.set(qn("w:type"), "dxa")


def set_table_borders(table, color="A9B4BC", size=5):
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
        tag.set(qn("w:sz"), str(size))
        tag.set(qn("w:space"), "0")
        tag.set(qn("w:color"), color)


def set_table_geometry(table, widths_cm):
    total_twips = int(sum(widths_cm) / 2.54 * 1440)
    tbl_pr = table._tbl.tblPr
    tbl_w = tbl_pr.first_child_found_in("w:tblW")
    if tbl_w is None:
        tbl_w = OxmlElement("w:tblW")
        tbl_pr.append(tbl_w)
    tbl_w.set(qn("w:w"), str(total_twips))
    tbl_w.set(qn("w:type"), "dxa")
    tbl_layout = tbl_pr.first_child_found_in("w:tblLayout")
    if tbl_layout is None:
        tbl_layout = OxmlElement("w:tblLayout")
        tbl_pr.append(tbl_layout)
    tbl_layout.set(qn("w:type"), "fixed")

    grid = table._tbl.tblGrid
    for child in list(grid):
        grid.remove(child)
    for width in widths_cm:
        col = OxmlElement("w:gridCol")
        col.set(qn("w:w"), str(int(width / 2.54 * 1440)))
        grid.append(col)
    table.autofit = False
    for row in table.rows:
        for idx, (cell, width) in enumerate(zip(row.cells, widths_cm)):
            cell.width = Cm(width)
            tc_pr = cell._tc.get_or_add_tcPr()
            tc_w = tc_pr.first_child_found_in("w:tcW")
            if tc_w is None:
                tc_w = OxmlElement("w:tcW")
                tc_pr.append(tc_w)
            tc_w.set(qn("w:w"), str(int(width / 2.54 * 1440)))
            tc_w.set(qn("w:type"), "dxa")


def add_page_number(paragraph):
    paragraph.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    run = paragraph.add_run()
    fld_char1 = OxmlElement("w:fldChar")
    fld_char1.set(qn("w:fldCharType"), "begin")
    instr_text = OxmlElement("w:instrText")
    instr_text.set(qn("xml:space"), "preserve")
    instr_text.text = " PAGE "
    fld_char2 = OxmlElement("w:fldChar")
    fld_char2.set(qn("w:fldCharType"), "end")
    run._r.append(fld_char1)
    run._r.append(instr_text)
    run._r.append(fld_char2)
    set_run(run, size=9)


doc = Document()
sec = doc.sections[0]
sec.page_width = Cm(21.0)
sec.page_height = Cm(29.7)
sec.top_margin = Cm(3.2)
sec.bottom_margin = Cm(1.55)
sec.left_margin = Cm(2.0)
sec.right_margin = Cm(2.0)
sec.header_distance = Cm(1.0)
sec.footer_distance = Cm(0.65)
sec.different_first_page_header_footer = False
add_page_number(sec.footer.paragraphs[0])

styles = doc.styles
normal = styles["Normal"]
normal.font.name = "Times New Roman"
normal._element.rPr.rFonts.set(qn("w:eastAsia"), "宋体")
normal.font.size = Pt(12)
normal.paragraph_format.line_spacing_rule = WD_LINE_SPACING.EXACTLY
normal.paragraph_format.line_spacing = Pt(22)
normal.paragraph_format.space_before = Pt(0)
normal.paragraph_format.space_after = Pt(0)
normal.paragraph_format.first_line_indent = Pt(24)
normal.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY

for name, size, before, after in [
    ("Heading 1", 16, 0, 6),
    ("Heading 2", 13, 5, 2),
    ("Heading 3", 12, 3, 0),
]:
    style = styles[name]
    style.font.name = "Times New Roman"
    style._element.rPr.rFonts.set(qn("w:eastAsia"), "黑体")
    style.font.size = Pt(size)
    style.font.bold = True
    style.font.color.rgb = RGBColor.from_string("000000")
    style.paragraph_format.first_line_indent = Pt(0)
    style.paragraph_format.space_before = Pt(before)
    style.paragraph_format.space_after = Pt(after)
    style.paragraph_format.keep_with_next = True
    style.paragraph_format.line_spacing_rule = WD_LINE_SPACING.EXACTLY
    style.paragraph_format.line_spacing = Pt(22)

doc.core_properties.title = "车载平衡滚球运动控制系统设计报告"
doc.core_properties.subject = "2026年全国大学生电子设计竞赛H题"
doc.core_properties.author = ""
doc.core_properties.last_modified_by = ""
doc.core_properties.comments = ""


def add_p(text="", *, align=WD_ALIGN_PARAGRAPH.JUSTIFY, indent=True,
          size=12, bold=False, east="宋体", latin="Times New Roman",
          before=0, after=0, line=22, keep=False, color=None):
    p = doc.add_paragraph()
    p.alignment = align
    p.paragraph_format.first_line_indent = Pt(24 if indent else 0)
    p.paragraph_format.space_before = Pt(before)
    p.paragraph_format.space_after = Pt(after)
    p.paragraph_format.line_spacing_rule = WD_LINE_SPACING.EXACTLY
    p.paragraph_format.line_spacing = Pt(line)
    p.paragraph_format.keep_with_next = keep
    r = p.add_run(text)
    set_run(r, size=size, bold=bold, east=east, latin=latin, color=color)
    return p


def add_mixed(parts, *, align=WD_ALIGN_PARAGRAPH.JUSTIFY, indent=True,
              before=0, after=0, line=22):
    p = doc.add_paragraph()
    p.alignment = align
    p.paragraph_format.first_line_indent = Pt(24 if indent else 0)
    p.paragraph_format.space_before = Pt(before)
    p.paragraph_format.space_after = Pt(after)
    p.paragraph_format.line_spacing_rule = WD_LINE_SPACING.EXACTLY
    p.paragraph_format.line_spacing = Pt(line)
    for text, kwargs in parts:
        r = p.add_run(text)
        set_run(r, **kwargs)
    return p


def add_heading(text, level=1):
    p = doc.add_paragraph(style=f"Heading {level}")
    p.paragraph_format.keep_with_next = True
    r = p.add_run(text)
    set_run(r, size={1: 16, 2: 13, 3: 12}[level], bold=True, east="黑体")
    return p


def add_eq(text):
    return add_p(text, align=WD_ALIGN_PARAGRAPH.CENTER, indent=False,
                 size=11.5, east="宋体", latin="Cambria Math", before=1, after=1, line=20)


def add_caption(text):
    return add_p(text, align=WD_ALIGN_PARAGRAPH.CENTER, indent=False,
                 size=10.5, before=0, after=2, line=18)


def add_figure(path, width_cm, caption):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.first_line_indent = Pt(0)
    p.paragraph_format.space_before = Pt(2)
    p.paragraph_format.space_after = Pt(0)
    p.paragraph_format.line_spacing_rule = WD_LINE_SPACING.SINGLE
    p.paragraph_format.line_spacing = 1.0
    p.paragraph_format.keep_with_next = True
    r = p.add_run()
    r.add_picture(str(path), width=Cm(width_cm))
    add_caption(caption)


def add_table(headers, rows, widths_cm, font_size=10.5, header_size=10.5,
              alignments=None):
    table = doc.add_table(rows=1, cols=len(headers))
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = False
    set_table_geometry(table, widths_cm)
    set_table_borders(table)
    hdr = table.rows[0]
    set_repeat_table_header(hdr)
    for i, text in enumerate(headers):
        cell = hdr.cells[i]
        set_cell_shading(cell, "E8EEF2")
        set_cell_margins(cell)
        cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
        p = cell.paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.paragraph_format.first_line_indent = Pt(0)
        p.paragraph_format.space_before = Pt(0)
        p.paragraph_format.space_after = Pt(0)
        p.paragraph_format.line_spacing_rule = WD_LINE_SPACING.EXACTLY
        p.paragraph_format.line_spacing = Pt(16)
        r = p.add_run(str(text))
        set_run(r, size=header_size, bold=True, east="黑体")
    for row_idx, row in enumerate(rows):
        cells = table.add_row().cells
        for col_idx, text in enumerate(row):
            cell = cells[col_idx]
            if row_idx % 2 == 1:
                set_cell_shading(cell, "F8FAFB")
            set_cell_margins(cell)
            cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
            p = cell.paragraphs[0]
            p.alignment = (alignments[col_idx] if alignments else
                           (WD_ALIGN_PARAGRAPH.CENTER if col_idx == 0 else WD_ALIGN_PARAGRAPH.LEFT))
            p.paragraph_format.first_line_indent = Pt(0)
            p.paragraph_format.space_before = Pt(0)
            p.paragraph_format.space_after = Pt(0)
            p.paragraph_format.line_spacing_rule = WD_LINE_SPACING.EXACTLY
            p.paragraph_format.line_spacing = Pt(16)
            r = p.add_run(str(text))
            set_run(r, size=font_size)
    p_after = doc.add_paragraph()
    p_after.paragraph_format.space_before = Pt(0)
    p_after.paragraph_format.space_after = Pt(0)
    p_after.paragraph_format.line_spacing = Pt(4)
    return table


def page_break():
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(0)
    p.paragraph_format.space_after = Pt(0)
    p.add_run().add_break(WD_BREAK.PAGE)


# Sealed cover — follow the official binding sample; no contestant information.
add_p("2026年全国大学生电子设计竞赛", align=WD_ALIGN_PARAGRAPH.CENTER,
      indent=False, size=16, bold=True, east="黑体", before=42, after=3, line=24)
add_p("河南赛区赛暨模拟电子系统设计专题赛选拔赛", align=WD_ALIGN_PARAGRAPH.CENTER,
      indent=False, size=13, bold=True, east="黑体", after=14, line=22)
add_p("（H题）", align=WD_ALIGN_PARAGRAPH.CENTER, indent=False,
      size=15, bold=True, east="黑体", after=10, line=24)
add_p("【本科组】", align=WD_ALIGN_PARAGRAPH.CENTER, indent=False,
      size=15, bold=True, east="黑体", after=26, line=24, color="D00000")

p_logo = doc.add_paragraph()
p_logo.alignment = WD_ALIGN_PARAGRAPH.CENTER
p_logo.paragraph_format.first_line_indent = Pt(0)
p_logo.paragraph_format.space_before = Pt(0)
p_logo.paragraph_format.space_after = Pt(34)
p_logo.paragraph_format.line_spacing_rule = WD_LINE_SPACING.SINGLE
p_logo.add_run().add_picture(str(OFFICIAL_LOGO), width=Cm(1.75))

add_p("2026年7月31日", align=WD_ALIGN_PARAGRAPH.CENTER, indent=False,
      size=12, bold=True, east="宋体", after=0, line=20)

page_break()

# Abstract
add_heading("摘  要", 1)
abstract = (
    "本系统面向环形黑线轨道上的车载滚球控制任务，以STM32F407为主控制器，"
    "利用四路红外巡线模块和车轮编码器完成差速循迹；K230通过固定相机识别钢球位置，"
    "并同步输出RTSP视频。滚球机构采用步进电机驱动齿轮齿条升降球杆右端，"
    "控制器由位置—速度外环、执行机构位置内环和车体加速度前馈构成。"
    "软件按调平、待启动、运行、视觉丢失、限位和急停组织状态，针对六项任务设置独立参数。"
    "四路模块的GPIO极性、引脚映射、程序编译与下载已完成核对；视觉推流及通信协议已通过分项测试。"
    "整车循迹圈速和滚球误差将在联合测试后按重复试验结果填入。"
)
add_p(abstract, indent=True, before=6, after=10)
add_mixed([
    ("关键词：", {"size": 12, "bold": True, "east": "黑体"}),
    ("红外循迹；机器视觉；滚球平衡；齿轮齿条；双闭环控制",
     {"size": 12, "east": "宋体"}),
], indent=False, align=WD_ALIGN_PARAGRAPH.LEFT)

page_break()

# Body page 1
add_heading("1  系统方案与总体设计", 1)
add_heading("1.1  任务分解与设计取舍", 2)
add_p(
    "本题并非单一循迹问题。小车要在直线段和半圆段之间切换速度，又要在车体纵向加速度持续变化时把钢球限制在球杆指定位置；"
    "图传还必须独立、连续，不得因录像或显示阻塞控制。由此将系统拆成三条链路：底盘运动链、滚球姿态链和图像传输链。"
    "控制量只在车载端闭环生成，场外接收端仅显示并保存视频。"
)
add_table(
    ["环节", "备选方案", "取舍依据", "采用方案"],
    [
        ["循迹检测", "摄像头；模拟灰度阵列；四路数字红外", "赛题限定光电传感器；GPIO链路短、阈值可现场调整，但需处理四路分辨率和交界歧义", "Hiwonder四路模块，S1～S4数字输入"],
        ["钢球检测", "颜色/轮廓法；轻量目标检测", "金属反光使阈值随背景变化；训练后的检测器对局部高光更稳", "K230运行YOLO11，像素坐标标定为毫米"],
        ["球杆执行", "舵机曲柄；丝杆；步进齿轮齿条", "需双向快速、小回差且便于限位；丝杆响应偏慢", "步进电机+预紧齿轮齿条升降右端"],
        ["控制架构", "多控制器分工；单主控+视觉协处理", "底盘与滚球的时间基准应统一，减少跨板同步误差", "STM32F407主控，K230负责视觉与推流"],
    ],
    [2.1, 4.0, 6.0, 4.9],
    font_size=9.7, header_size=10,
)
add_heading("1.2  总体结构", 2)
add_p(
    "四路红外巡线模块从车体左至右输出S1～S4状态，编码器提供左右轮速度；STM32F407据此计算差速指令。"
    "相机覆盖整根25 cm球杆，K230输出钢球坐标、置信度和帧状态。主控将坐标送入滚球控制器，"
    "再以STEP/DIR脉冲驱动升降机构。图传码流由K230通过2.4 GHz无线网络送到场外接收端，"
    "因此显示、录像与运动控制互不抢占实时资源。"
)
add_figure(FIG1, 16.4, "图1  系统总体框图")

page_break()

# Body page 2
add_heading("2  小车循迹与运动控制理论", 1)
add_heading("2.1  差速运动模型", 2)
add_p(
    "设左右轮线速度为vL、vR，轮距为B，则车体前向速度v和偏航角速度ω满足："
)
add_eq("v = (vR + vL) / 2，      ω = (vR - vL) / B")
add_p(
    "实物使用Hiwonder Line Follower(4ch) V2.0模块，四个探头的阈值可独立调整，安装高度取手册建议值附近的8 mm。"
    "实测数字极性为黑线1、白底0。记从左到右的状态为bi，权值wi依次取−30、−10、10、30；"
    "相邻有效探头组成候选组G，横向误差只在选中组内计算："
)
add_eq("e = Σ(i∈G)(wi·bi) / Σ(i∈G)bi，     wi = {−30，−10，10，30}")
add_p(
    "交界处可能出现1001一类的非相邻响应；直接平均会把误差拉回零点。程序按上一周期误差选择较近的候选组，"
    "距离相同再选探头较多的组。启动时锁存中心偏置，用于补偿安装零差。"
)
add_figure(FIG2, 16.2, "图2  循迹误差提取与速度调度")
add_heading("2.2  转向、速度与起停调度", 2)
add_p(
    "转向外环采用比例和误差增量项，不在转向通道积累积分。当前20 ms周期的离散表达式为："
)
add_eq("u = sat{[3ec + 2(ec - e₋₁)]/10，±14}，  nL* = n0 + u，  nR* = n0 - u")
add_p(
    "其中ec为扣除启动中心偏置并经±2死区处理后的误差。n0按偏差幅值分为20、17和14 count/周期三档；"
    "单独触发S1或S4时停内轮、以16 count/周期驱动外轮。左右轮各有编码器速度PI，"
    "其输出由前馈、3倍速度误差和限幅积分项组成。这样，循迹外环决定两轮目标差，速度内环负责克服电机差异。"
    "暂时丢线时按最后误差方向低速搜索，连续30个周期即0.6 s未恢复则停车。A点启动时锁存计时；"
    "再次进入停止窗口后分级制动。中线约6.14 m，20 s一圈需0.31 m/s平均速度，"
    "速度上限由弯道不脱线和滚球不越界共同确定。"
)

page_break()

# Body page 3
add_heading("3  滚球动力学与执行机构设计", 1)
add_heading("3.1  球杆模型与车体扰动", 2)
add_p(
    "钢球在球杆上近似纯滚动。忽略滚动阻力，实心球的转动惯量I=2mr²/5。沿杆方向列平动和转动方程，可得"
)
add_eq("m·x¨ = m·g·sinθ - F，   F·r = I·α，   α = x¨/r")
add_eq("x¨ = (5/7)g·sinθ ≈ (5/7)g·θ      （|θ|较小时）")
add_p(
    "小车加速时，车体坐标系中还存在与加速度方向相反的惯性项。控制器在位置反馈之外加入"
    "θff≈ax/g的前馈量，使球杆先抵消底盘加减速，再由反馈修正剩余误差。该项在起步和停车阶段最重要，"
    "也解释了为何静止调好的参数不能直接照搬到整圈运行。"
)
add_figure(FIG3, 16.2, "图3  球杆升降机构与变量定义")
add_heading("3.2  机构尺寸与控制量", 2)
add_p(
    "球杆有效长度L=250 mm，中心线距车体平台不低于50 mm；右端升高h时有θ=arcsin(h/L)。例如h=13 mm对应约3.0°，"
    "已能产生约0.37 m/s²的滚球加速度。右端由步进电机、小齿轮和竖直齿条驱动，齿条另设直线导向，"
    "不让电机轴承承担侧向载荷。升降接头设置短水平补偿行程，以释放倾斜时的投影变化；两端机械限位限定最大倾角。"
)
add_heading("3.3  串级控制", 2)
add_p(
    "外环由钢球位置误差和测得速度生成目标步数s*，内环把s*换成步进速度。现有控制框架采用"
    "s*=sat[2(x−x*)+v/6, ±160]，执行机构以位置比例控制并限制加速度；每个内环周期速度最多改变25 step/s，"
    "软位置范围为±220 step。静止±50 mm任务锁定给定值；动态居中任务取x*=0；任意位置任务在启动键按下时"
    "对若干帧位置取中值并锁存为x*。"
)

page_break()

# Body page 4
add_heading("4  硬件电路设计", 1)
add_heading("4.1  主控、驱动与传感接口", 2)
add_p(
    "主控制器选用STM32F407VET6。该器件的定时器资源可并行完成双路编码器、双路电机PWM、步进脉冲和10 ms控制节拍；"
    "浮点运算能力也足以在一个周期内完成滤波、状态判定与串级控制。底盘采用TB6612双H桥。"
    "巡线器件为四路反射式红外光电模块，手册给出的工作电压为3.3～5 V、检测距离为1 mm～10 cm；"
    "本机按约8 mm安装，并逐路调整阈值。内侧S2、S3用于精确压线，外侧S1、S4用于提前识别急弯。"
    "控制板和机构均布置在350 mm×250 mm整车外廓内。"
)
add_table(
    ["功能", "器件/接口", "关键连接与参数"],
    [
        ["左轮驱动", "TB6612 / TIM1_CH3", "PE13输出PWM，PD0、PD1控制方向"],
        ["右轮驱动", "TB6612 / TIM1_CH4", "PE14输出PWM，PD2、PD3控制方向，PD4为STBY"],
        ["轮速反馈", "增量编码器 / TIM3、TIM4", "左PB4/PB5，右PB6/PB7，硬件正交计数"],
        ["球杆执行", "DCC-100 V3 / TIM6", "PA6 STEP、PA7 DIR、PA5 SLP、PA4 RST；脉冲调度20 kHz"],
        ["钢球数据", "K230 / USART2_RX", "PA3接收3.3 V TTL，115200 bit/s，帧带CRC16"],
        ["循迹输入", "Hiwonder四路巡线模块 / GPIO", "左→右：S1/PB1、S2/PB0、S3/PC5、S4/PC4；实测黑=1、白=0"],
    ],
    [2.3, 5.0, 9.7],
    font_size=9.7, header_size=10,
)
add_heading("4.2  视觉与图传", 2)
add_p(
    "K230固定在车上，摄像头视场覆盖整根球杆。采集分辨率为320×180，检测网络输入320×320；"
    "检测结果经分段线性标定映射为−125~+125 mm。相同图像叠加检测框后编码为RTSP码流，"
    "经车载2.4 GHz无线热点发送，场外计算机负责显示和保存全程录像。推流帧率约20 frame/s、码率约1.2 Mbit/s，"
    "控制报文与视频码流分离。"
)
add_heading("4.3  供电与保护", 2)
add_p(
    "动力电池分别向电机母线、5 V视觉计算单元和3.3 V逻辑域供电。电机驱动与数字电路就近布置退耦，"
    "各支路回流在电池负端单点汇接，减小步进脉冲和直流电机换向对串口、GPIO采样的干扰。"
    "巡线模块允许3.3～5 V供电，控制端按3.3 V逻辑域布线；四个电位器只在标定时调整，运行中不改阈值。"
    "软件软限位、机械挡块、驱动休眠和急停键形成四级保护；上电默认关闭电机，完成调平与传感器有效性检查后才允许进入任务状态。"
)

page_break()

# Body page 5
add_heading("5  软件与通信设计", 1)
add_heading("5.1  分层实时任务", 2)
add_p(
    "程序以状态机组织，而不是在主循环中串行执行所有动作。20 kHz中断只生成步进脉冲；滚球外环以10 ms更新，"
    "四路巡线与编码器速度环以20 ms更新；视觉报文到达时仅做快速解析并更新缓存；OLED以150 ms刷新。"
    "图传、显示和串口突发数据均不改变电机控制周期。"
)
add_figure(FIG4, 14.7, "图4  主控状态机及异常退化路径")
add_heading("5.2  视觉数据帧与时效判定", 2)
add_p(
    "数据帧采用帧头、长度、序号、钢球毫米坐标、置信度、标志位、图像年龄和CRC16/CCITT-FALSE校验。"
    "主控检查长度、序号连续性、CRC和时间戳；只有检测有效且年龄未超限的数据才进入控制。"
    "连续200 ms无有效坐标时，滚球目标以限速方式回到水平，底盘按任务类型减速或停车。"
)
add_table(
    ["字段", "作用", "异常处理"],
    [
        ["帧头/长度", "快速同步并限定报文边界", "不匹配则逐字节重新找帧头"],
        ["序号/年龄", "识别丢帧、旧帧和缓存堆积", "旧数据只统计，不进入反馈"],
        ["x_mm/置信度", "给出钢球位置及检测可信度", "越界或低置信度保持上一安全状态"],
        ["flags", "携带检测、标定和相机状态", "相机异常进入视觉丢失状态"],
        ["CRC16", "发现传输位翻转与错位", "丢弃整帧，等待下一有效帧"],
    ],
    [3.0, 7.2, 6.8],
    font_size=9.8, header_size=10,
)
add_heading("5.3  关键程序流程", 2)
add_p(
    "启动键按下后，程序读取题号、锁存目标并清零编码器与积分量。控制周期依次执行："
    "读取S1～S4→选择相邻有效组→计算横向误差与轮速目标→检查视觉时效→计算滚球外环→执行步进限速→检查终点与保护条件。"
    "保护条件优先，恢复也须连续确认，避免状态在临界点反复跳变。"
)

page_break()

# Body page 6
add_heading("6  分题控制策略与参数整定", 1)
add_heading("6.1  六项任务的统一实现", 2)
add_p(
    "六项要求共用同一套底层驱动与保护逻辑，差别只体现在目标位置、车辆速度上限和停车条件。"
    "这样可减少现场切换程序造成的参数遗漏。任务由按键选择，OLED只显示题号、目标位置、视觉状态和计时等必要信息，屏幕尺寸不超过2英寸。"
)
add_table(
    ["任务", "车辆策略", "滚球目标/图传策略", "结束条件"],
    [
        ["(1) 图传", "车辆静止，关闭轮驱动", "持续检测并推流，场外全程录像", "人工停止录像"],
        ["(2) 快速一圈", "直线高速、弯道分级降速", "球杆保持机械水平", "A点窗口内减速停车"],
        ["(3) ±5 cm", "车辆静止", "0→+50→−50 mm；到位后反向制动", "各位置稳定后记录时间与误差"],
        ["(4) A至过B", "起步前馈，目标8 s内越过B", "x*=0，限制|x|≤10 mm", "越过B点标志"],
        ["(5) 居中一圈", "平滑加减速，一圈≤30 s", "x*=0，全程记录最大偏差", "回到A点停车"],
        ["(6) 任意点一圈", "同任务(5)，保留更大控制余量", "启动时中值锁存x*=x0", "回到A点停车"],
    ],
    [1.5, 4.9, 7.0, 3.6],
    font_size=9.4, header_size=9.7,
)
add_heading("6.2  整定顺序", 2)
add_p(
    "整定从“机构—静态—动态”逐层推进。先在无球条件下核对步进方向、软限位和水平零点；放球后只开执行机构内环，"
    "消除齿条松动并确定安全速度。随后在车辆静止时整定位置—速度外环，使±50 mm转换无持续振荡。"
    "最后把车速从低档逐级上调，观察起步、弯道和停车三处峰值误差，再增加车体加速度前馈。"
)
add_heading("6.3  图像标定与任意位置锁存", 2)
add_p(
    "在球杆左端、中心和右端分别放置钢球，记录像素坐标37、351、658并按左右两段线性映射。"
    "分段标定可吸收相机透视和安装偏角。任务(6)按下启动键后采集不少于9个有效位置，去掉低置信度帧并取中值；"
    "目标锁存后不再随检测噪声漂移。若目标靠近机械端部，则自动收紧车速和倾角上限。"
)

page_break()

# Body page 7
add_heading("7  测试方案与阶段结果", 1)
add_heading("7.1  测试条件与判据", 2)
add_p(
    "测试在题目尺寸的黑线环形轨道上进行。直尺用于校核A点停车和钢球位置，秒表与主控计时器交叉记录用时；"
    "场外接收端保存RTSP视频，便于逐帧复核最大偏差出现的工况。每项最终数据应至少重复5次，"
    "报告平均值、最大值和失败次数，不能只保留最好的一次。"
)
add_table(
        ["已完成检查", "当前记录", "判定"],
    [
        ["主控滚球安全框架", "工程编译0错误、0警告；下载校验通过；10 ms控制与限位状态可运行", "阶段通过"],
        ["底盘循迹", "四路GPIO极性及S1～S4引脚已实物核对；V1工程编译0错误、0警告并完成下载校验", "接口通过，整圈待测"],
        ["K230视觉与图传", "YOLO11检测、LCD叠加显示及RTSP推流已在视觉端运行", "单机链路通过"],
        ["串口协议", "PC侧完成CRC向量、分包、错帧和恢复测试", "协议通过，实物长时测试待做"],
        ["整车滚球性能", "控制模型、机构和程序接口已完成，尚未取得完整联合测试数据", "待联合实测"],
    ],
    [4.0, 9.7, 3.3],
    font_size=9.8, header_size=10,
)
add_heading("7.2  最终测试记录表", 2)
add_p(
    "下表保留为联调后的唯一数据入口。填表时同时核对录像和主控日志；“最大误差”取全程绝对值最大者。"
)
add_table(
    ["项目", "次数", "平均用时/s", "最大误差/mm", "成功率", "结果"],
    [
        ["任务(2) 一圈并停车", "≥5", "待实测", "停车：待实测", "待实测", "目标≤20 s、≤20 mm"],
        ["任务(3) +50 mm", "≥5", "待实测", "待实测", "待实测", "目标≤5 s、≤10 mm"],
        ["任务(3) −50 mm", "≥5", "待实测", "待实测", "待实测", "目标≤5 s、≤10 mm"],
        ["任务(4) A至过B", "≥5", "待实测", "滚球：待实测", "待实测", "目标≤8 s、≤10 mm"],
        ["任务(5) 居中一圈", "≥5", "待实测", "滚球：待实测", "待实测", "目标≤30 s、≤10 mm"],
        ["任务(6) 任意点一圈", "≥5", "待实测", "滚球：待实测", "待实测", "目标≤30 s、≤10 mm"],
    ],
    [4.4, 1.4, 3.0, 3.2, 2.2, 3.8],
    font_size=9.2, header_size=9.5,
)
add_heading("7.3  联调时优先观察的波形", 2)
add_p(
    "日志同步保存车辆速度v、循迹误差e、钢球位置x、目标x*、球杆指令s*和视觉帧年龄。"
    "若x在起步瞬间同向偏离，先调加速度前馈；若静止时周期摆动，降低外环增益或增加速度阻尼；"
    "若弯道误差突增但直线正常，则应先收紧车速调度，而不是继续提高滚球增益。"
)

page_break()

# Body page 8
add_heading("8  误差分析与结论", 1)
add_heading("8.1  误差来源与抑制", 2)
add_p(
    "钢球位置误差主要由视觉量化、标定、通信时延、机构回差和底盘扰动组成。若各项近似独立，"
    "可用均方根形式估算总误差："
)
add_eq("σx ≈ √(σ²pix + σ²cal + (vball·τ)² + σ²backlash + σ²body)")
add_table(
    ["误差源", "表现", "抑制措施"],
    [
        ["像素与反光", "检测框中心跳动，端部误差增大", "固定曝光；多帧中值；置信度门限；三点分段标定"],
        ["通信时延", "快速滚动时位置反馈滞后", "115200 bit/s短帧；序号/年龄检查；速度估计与预测"],
        ["齿侧间隙", "换向后短时无动作", "齿条预紧；换向补偿；导向机构承受侧力"],
        ["车体加减速", "起步、弯道、停车时球偏移", "S形速度给定；加速度前馈；弯道降速"],
        ["四路巡线歧义", "交界处出现两个不相邻有效组", "相邻探头分组；按上一位置择近；起步中心偏置锁存"],
    ],
    [3.1, 6.1, 8.8],
    font_size=9.7, header_size=10,
)
add_heading("8.2  设计结论", 2)
add_p(
    "本设计把循迹、滚球和图传拆成独立但共享时基的子系统。四路巡线状态经相邻分组和历史位置约束后，"
    "能够避免环形轨道交界处的多组响应被错误平均；"
    "齿轮齿条机构提供可限位、可重复的球杆倾角；滚球控制通过位置—速度串级和底盘加速度前馈兼顾静态定位与动态行驶。"
    "现阶段四路巡线的极性与引脚映射已经实物核对，V1底盘程序完成无警告编译和下载；"
    "视觉端检测推流、滚球安全框架和协议软件测试也已形成可复查记录。"
    "尚未完成的部分集中在整车滚球联合测试，后续只需按第7页表格补齐重复试验数据，并据峰值误差调整前馈与速度档位。"
)
add_heading("参考文献", 2)
refs = [
    "[1] 全国大学生电子设计竞赛组织委员会. H题：车载平衡滚球运动控制系统[Z]. 2026.",
    "[2] O G A T A K. Modern Control Engineering[M]. 5th ed. Prentice Hall, 2010.",
    "[3] STMicroelectronics. STM32F407xx Datasheet[EB/OL].",
    "[4] Toshiba. TB6612FNG Dual DC Motor Driver Datasheet[EB/OL].",
    "[5] 嘉楠科技. K230/CanMV开发文档[EB/OL].",
    "[6] Hiwonder. Line Follower(4ch) V2.0使用手册[Z].",
]
for ref in refs:
    add_p(ref, indent=False, size=10.5, line=18)


# Set keep-with-next for figure captions and prevent table row splitting.
for table in doc.tables:
    for row in table.rows:
        tr_pr = row._tr.get_or_add_trPr()
        cant_split = OxmlElement("w:cantSplit")
        tr_pr.append(cant_split)

# Update fields on open/print.
settings = doc.settings._element
update_fields = settings.find(qn("w:updateFields"))
if update_fields is None:
    update_fields = OxmlElement("w:updateFields")
    settings.append(update_fields)
update_fields.set(qn("w:val"), "true")

doc.save(OUT_DOCX)

cn_count = sum(1 for ch in abstract if "\u4e00" <= ch <= "\u9fff")
print(f"Created: {OUT_DOCX}")
print(f"Abstract Chinese characters: {cn_count}")
print(f"Figures: {FIG_DIR}")
