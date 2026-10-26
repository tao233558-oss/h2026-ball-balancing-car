from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate,
    Frame,
    KeepTogether,
    LongTable,
    PageBreak,
    PageTemplate,
    Paragraph,
    Spacer,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "output" / "pdf" / "Tianmengxing_MSPM0G3507_car_complete_wiring_v4.pdf"
OUT.parent.mkdir(parents=True, exist_ok=True)

pdfmetrics.registerFont(TTFont("CN", r"C:\Windows\Fonts\msyh.ttc"))
pdfmetrics.registerFont(TTFont("CN-Bold", r"C:\Windows\Fonts\msyhbd.ttc"))

PAGE_W, PAGE_H = A4
MARGIN_L = 14 * mm
MARGIN_R = 14 * mm
MARGIN_T = 17 * mm
MARGIN_B = 15 * mm

NAVY = colors.HexColor("#16324F")
BLUE = colors.HexColor("#276FBF")
LIGHT_BLUE = colors.HexColor("#EAF3FB")
RED = colors.HexColor("#C62828")
LIGHT_RED = colors.HexColor("#FDECEC")
ORANGE = colors.HexColor("#E36C09")
LIGHT_ORANGE = colors.HexColor("#FFF1E6")
GREEN = colors.HexColor("#26734D")
LIGHT_GREEN = colors.HexColor("#EAF6EF")
GRAY = colors.HexColor("#5D6770")
LIGHT_GRAY = colors.HexColor("#F3F5F7")
GRID = colors.HexColor("#B7C0C8")


styles = getSampleStyleSheet()
styles.add(ParagraphStyle(
    name="CNBody", fontName="CN", fontSize=8.4, leading=12.2,
    textColor=colors.HexColor("#20262C"), spaceAfter=4,
))
styles.add(ParagraphStyle(
    name="CNSmall", parent=styles["CNBody"], fontSize=7.2, leading=10.2,
))
styles.add(ParagraphStyle(
    name="CNTiny", parent=styles["CNBody"], fontSize=6.5, leading=8.7,
))
styles.add(ParagraphStyle(
    name="CNTitle", fontName="CN-Bold", fontSize=21, leading=28,
    alignment=TA_CENTER, textColor=NAVY, spaceAfter=6,
))
styles.add(ParagraphStyle(
    name="CNSubtitle", fontName="CN", fontSize=10.5, leading=15,
    alignment=TA_CENTER, textColor=GRAY, spaceAfter=10,
))
styles.add(ParagraphStyle(
    name="CNH1", fontName="CN-Bold", fontSize=14, leading=19,
    textColor=NAVY, spaceBefore=3, spaceAfter=7,
))
styles.add(ParagraphStyle(
    name="CNH2", fontName="CN-Bold", fontSize=10.5, leading=14,
    textColor=BLUE, spaceBefore=5, spaceAfter=4,
))
styles.add(ParagraphStyle(
    name="CNBox", fontName="CN", fontSize=8.2, leading=12,
    textColor=colors.HexColor("#20262C"),
))
styles.add(ParagraphStyle(
    name="CNBoxBold", parent=styles["CNBox"], fontName="CN-Bold",
))
styles.add(ParagraphStyle(
    name="CNCell", fontName="CN", fontSize=6.6, leading=8.4,
    textColor=colors.HexColor("#20262C"),
))
styles.add(ParagraphStyle(
    name="CNCellBold", parent=styles["CNCell"], fontName="CN-Bold",
))
styles.add(ParagraphStyle(
    name="CNCellHeader", parent=styles["CNCell"], fontName="CN-Bold", textColor=colors.white,
))
styles.add(ParagraphStyle(
    name="CNCellRed", parent=styles["CNCell"], fontName="CN-Bold", textColor=RED,
))
styles.add(ParagraphStyle(
    name="CNCellGreen", parent=styles["CNCell"], fontName="CN-Bold", textColor=GREEN,
))


def P(text, style="CNBody"):
    return Paragraph(str(text), styles[style])


def tag(text, color=RED):
    return Paragraph(f'<font color="{color.hexval()}"><b>{text}</b></font>', styles["CNCell"])


def page_header_footer(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(colors.HexColor("#D7DDE2"))
    canvas.setLineWidth(0.45)
    canvas.line(MARGIN_L, PAGE_H - 11.5 * mm, PAGE_W - MARGIN_R, PAGE_H - 11.5 * mm)
    canvas.setFont("CN", 6.8)
    canvas.setFillColor(GRAY)
    canvas.drawString(MARGIN_L, PAGE_H - 9.2 * mm, "天猛星 MSPM0G3507 电赛小车完整接线基线")
    canvas.drawRightString(PAGE_W - MARGIN_R, PAGE_H - 9.2 * mm, "v4.0 · 2026-07-16")
    canvas.line(MARGIN_L, 10.2 * mm, PAGE_W - MARGIN_R, 10.2 * mm)
    canvas.drawString(MARGIN_L, 6.6 * mm, "依据：car_final_ccs 源码 + MSPM0G3507 数据手册 + 用户提供的天猛星排针图")
    canvas.drawRightString(PAGE_W - MARGIN_R, 6.6 * mm, f"第 {doc.page} 页")
    canvas.restoreState()


frame = Frame(
    MARGIN_L, MARGIN_B, PAGE_W - MARGIN_L - MARGIN_R, PAGE_H - MARGIN_T - MARGIN_B,
    leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0,
)
doc = BaseDocTemplate(
    str(OUT), pagesize=A4,
    leftMargin=MARGIN_L, rightMargin=MARGIN_R, topMargin=MARGIN_T, bottomMargin=MARGIN_B,
    title="天猛星 MSPM0G3507 电赛小车完整接线与旧 PCB 改线表",
    author="Codex",
)
doc.addPageTemplates([PageTemplate(id="main", frames=[frame], onPage=page_header_footer)])


def box(text, bg, border, style="CNBox", pad=7):
    t = Table([[P(text, style)]], colWidths=[PAGE_W - MARGIN_L - MARGIN_R])
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), bg),
        ("BOX", (0, 0), (-1, -1), 0.8, border),
        ("LEFTPADDING", (0, 0), (-1, -1), pad),
        ("RIGHTPADDING", (0, 0), (-1, -1), pad),
        ("TOPPADDING", (0, 0), (-1, -1), pad),
        ("BOTTOMPADDING", (0, 0), (-1, -1), pad),
    ]))
    return t


def section_title(text):
    return P(text, "CNH1")


def make_table(rows, widths, header=True, font=6.6, row_bgs=None, repeat=1):
    cooked = []
    for r, row in enumerate(rows):
        cooked_row = []
        for cell in row:
            if isinstance(cell, (Paragraph, Table)):
                cooked_row.append(cell)
            else:
                cooked_row.append(P(cell, "CNCellHeader" if header and r == 0 else "CNCell"))
        cooked.append(cooked_row)
    t = LongTable(cooked, colWidths=widths, repeatRows=repeat if header else 0, hAlign="LEFT")
    commands = [
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("GRID", (0, 0), (-1, -1), 0.35, GRID),
        ("LEFTPADDING", (0, 0), (-1, -1), 3),
        ("RIGHTPADDING", (0, 0), (-1, -1), 3),
        ("TOPPADDING", (0, 0), (-1, -1), 2.2),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 2.2),
    ]
    if header:
        commands += [
            ("BACKGROUND", (0, 0), (-1, 0), NAVY),
            ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
            ("FONTNAME", (0, 0), (-1, 0), "CN-Bold"),
        ]
        start = 1
    else:
        start = 0
    for i in range(start, len(rows)):
        commands.append(("BACKGROUND", (0, i), (-1, i), colors.white if i % 2 else LIGHT_GRAY))
    if row_bgs:
        for idx, color in row_bgs.items():
            commands.append(("BACKGROUND", (0, idx), (-1, idx), color))
    t.setStyle(TableStyle(commands))
    return t


story = []

# Page 1: outcome and delta-first guidance
story += [
    Spacer(1, 8 * mm),
    P("天猛星 MSPM0G3507", "CNTitle"),
    P("电赛小车完整接线 PDF · 含旧 PCB 逐项改线表", "CNTitle"),
    P("PCB v4.0 冻结目标 / DCC-100v3双轴云台已加入 / 无机械限位 / 3.3 V逻辑", "CNSubtitle"),
    box(
        "<b>文档状态：在 v3.2 基线上新增 DCC-100v3 双轴云台：PAN 保留 PA26/PA14，TILT 新增 PB4/PB5，两路 SLP 共用 PB17；用户确认不安装机械限位。</b> 蓝牙已迁移到 PB2/PB3，OLED+TOF 使用 PB0/PB1，因此 PB4/PB5 可专用于云台。DCC 诊断固件已编译，实板波形仍待验证。",
        LIGHT_ORANGE, ORANGE, "CNBox",
    ),
    Spacer(1, 5 * mm),
    section_title("先看这里：旧 PCB 必须修改的网络"),
]

delta_rows = [
    ["编号", "功能/旧 PCB 网络", "旧引脚或旧接法", "天猛星新引脚/新接法", "结论"],
    ["M1", "左电机 PWM / PWMA", "PB6", "PA29 · LQFP64 脚36 · TIMG6_C0", tag("已修改")],
    ["M2", "右电机 PWM / PWMB", "PB7", "PA30 · LQFP64 脚37 · TIMG6_C1", tag("已修改")],
    ["M3", "循迹 D7 / R3", "PA2；中间版PA22；后曾计划PA3", "PB25 · LQFP64 脚27 · 排针U7_7/U9_7", tag("再次修改")],
    ["M4", "蜂鸣器控制", "PB8", "PB12 · LQFP64 脚64 · 低有效", tag("已修改")],
    ["M5", "指示灯控制", "PB9", "PB13 · LQFP64 脚1 · 高有效", tag("已修改")],
    ["M6", "步进 STEP/PUL", "PA18", "PA26 · LQFP64 脚30", tag("已修改")],
    ["M7", "OLED/TOF I2C SCL", "OLED原PB2", "PB0 · LQFP64 脚47", tag("已修改")],
    ["M8", "OLED/TOF I2C SDA", "OLED原PB3", "PB1 · LQFP64 脚48", tag("已修改")],
    ["M9", "蓝牙 UART3", "原计划PB4/PB5", "PB2_TX / PB3_RX · 脚50/51", tag("已修改")],
    ["M10", "K230 UART2", "旧文档误建议UART0", "PB15_TX / PA22_RX · 脚3/18", tag("专用串口")],
    ["M11", "TOF200F", "临时UART PB4/PB5", "I2C PB0/PB1 · 与OLED共线", tag("接口修改")],
    ["C1", "TB6612 STBY", "若旧板直接接 3.3 V", "改接 PB16 · 脚4；建议 10 kΩ 下拉", tag("接法修改")],
    ["C2", "循迹 D2 / L3", "旧接线表曾写 PB15", "当前代码实际为 PA25 · 脚26", tag("必须复核")],
    ["C3", "DCC水平轴 PAN", "原单轴 PA26/PA14", "PWM1=PA26；DIR1=PA14", tag("保留旧线", GREEN)],
    ["M12", "DCC垂直轴 TILT", "旧PCB无第二轴", "PWM2=PB4；DIR2=PB5 · 脚52/53", tag("新增")],
    ["M13", "DCC两路休眠", "旧PCB无安全休眠控制", "SLP1+SLP2并接PB17 · 脚14；10k下拉", tag("新增")],
    ["C4", "机械限位", "未安装", "不分配GPIO；仅使用上电相对软限位", tag("已确认", GREEN)],
]
story.append(make_table(delta_rows, [10*mm, 38*mm, 34*mm, 66*mm, 26*mm], row_bgs={i: LIGHT_RED for i in range(1, len(delta_rows))}))
story += [
    Spacer(1, 4 * mm),
    box(
        "<b>最终串口方案：</b> PA10/PA11只给板载CH340；PA8/PA9给IMU；PB15/PA22给K230；PB2/PB3给蓝牙。TOF200F改用PB0/PB1 I2C并与OLED共线。<br/>"
        "<b>禁止沿用旧版：</b> K230不可再接PA10/PA11或PB1；TOF不可再占PB4/PB5；D7不可使用PA22或未引出的PA3，最终PCB目标是PB25。庐山派USB-A也不能直接插天猛星Type-C代替TTL串口。",
        LIGHT_RED, RED, "CNBox",
    ),
    Spacer(1, 5 * mm),
    P("标记说明：<font color='#C62828'><b>红色“已修改”</b></font> = 与旧 PCB 不同；<font color='#26734D'><b>保持</b></font> = 代码引脚未变；“复核” = 旧资料或板载资源存在冲突，必须回看旧原理图。", "CNBody"),
    P("适用范围：本文按“天猛星开发板作为核心板，插接到你的载板/扩展 PCB”编写。表中的 LQFP64 脚号用于芯片交叉核对，不等同于天猛星排针序号；封装和排针位置必须采用官方天猛星原理图/封装。", "CNBody"),
    PageBreak(),
]

# Page 2: complete signal table
story += [
    section_title("1. MCU 完整信号表（PCB v4.0目标；含DCC双轴云台）"),
    P("所有外接数字信号按 3.3 V 逻辑设计；方向均从 MSPM0 视角描述。红色行为迁移后修改项。", "CNSmall"),
]

pin_rows = [
    ["模块", "网络/功能", "MCU 引脚", "LQFP64", "方向/有效电平", "旧 PCB 对照", "载板接线要点"],
    ["TB6612", "PWMA 左 PWM", "PA29", "36", "输出 / TIMG6_C0", tag("PB6 → PA29"), "接 PWMA；33-100 Ω 可选串阻"],
    ["TB6612", "AIN1", "PB24", "23", "输出", tag("保持", GREEN), "接 AIN1"],
    ["TB6612", "AIN2", "PB20", "19", "输出", tag("保持", GREEN), "接 AIN2"],
    ["TB6612", "PWMB 右 PWM", "PA30", "37", "输出 / TIMG6_C1", tag("PB7 → PA30"), "接 PWMB；33-100 Ω 可选串阻"],
    ["TB6612", "BIN1", "PB19", "16", "输出", tag("保持", GREEN), "接 BIN1"],
    ["TB6612", "BIN2", "PB18", "15", "输出", tag("保持", GREEN), "接 BIN2"],
    ["TB6612", "STBY", "PB16", "4", "输出 / 高使能", tag("接法修改"), "不要硬接 3.3 V；10 kΩ 下拉"],
    ["左编码器", "ENC_L_A", "PA12", "5", "输入 / 中断 / 内部上拉", tag("保持", GREEN), "开集电极时外加 4.7-10 kΩ 上拉"],
    ["左编码器", "ENC_L_B", "PA13", "6", "输入 / 中断 / 代码内下拉", tag("保持", GREEN), "输出类型待确认；不要盲从软件下拉"],
    ["右编码器", "ENC_R_A", "PA24", "25", "输入 / 中断 / 内部上拉", tag("保持", GREEN), "开集电极时外加 4.7-10 kΩ 上拉"],
    ["右编码器", "ENC_R_B", "PA15", "8", "输入 / 中断 / 代码内下拉", tag("保持", GREEN), "输出类型待确认；不要盲从软件下拉"],
    ["8 路循迹", "D1 / L4", "PA0", "33", "输入 / 上拉 / 低有效", tag("保持", GREEN), "传感器输出必须为 3.3 V"],
    ["8 路循迹", "D2 / L3", "PA25", "26", "输入 / 上拉 / 低有效", tag("旧表错写 PB15"), "按代码接 PA25"],
    ["8 路循迹", "D3 / L2", "PA31", "39", "输入 / 上拉 / 低有效", tag("保持", GREEN), "传感器输出必须为 3.3 V"],
    ["8 路循迹", "D4 / L1", "PA28", "35", "输入 / 上拉 / 低有效", tag("保持", GREEN), "传感器输出必须为 3.3 V"],
    ["8 路循迹", "D5 / R1", "PA1", "34", "输入 / 上拉 / 低有效", tag("保持", GREEN), "传感器输出必须为 3.3 V"],
    ["8 路循迹", "D6 / R2", "PA7", "49", "输入 / 上拉 / 低有效", tag("保持", GREEN), "传感器输出必须为 3.3 V"],
    ["8 路循迹", "D7 / R3", "PB25", "27", "输入 / 上拉 / 低有效", tag("PA2→PA22→PA3→PB25"), "固件已同步并通过CCS编译"],
    ["8 路循迹", "D8 / R4", "PA16", "9", "输入 / 上拉 / 低有效", tag("保持", GREEN), "传感器输出必须为 3.3 V"],
    ["OLED+TOF", "I2C SCL", "PB0", "47", "开漏/释放高", tag("PB2 → PB0"), "两模块SCL共线；预留4.7k上拉"],
    ["OLED+TOF", "I2C SDA", "PB1", "48", "开漏/双向", tag("PB3 → PB1"), "两模块SDA共线；预留4.7k上拉"],
    ["IMU", "UART1 TX", "PA8", "54", "输出 / 9600", tag("保持", GREEN), "MCU TX → IMU RX"],
    ["IMU", "UART1 RX", "PA9", "55", "输入 / 9600", tag("保持", GREEN), "MCU RX ← IMU TX"],
    ["K230", "UART2 TX", "PB15", "3", "输出 / 115200", tag("新增专用"), "MCU TX → 庐山派物理13脚/GPIO6/RXD"],
    ["K230", "UART2 RX", "PA22", "18", "输入 / 115200", tag("新增专用"), "MCU RX ← 庐山派物理11脚/GPIO5/TXD"],
    ["蓝牙", "UART3 TX", "PB2", "50", "输出 / 速率待定", tag("PB4 → PB2"), "MCU TX → 蓝牙RX"],
    ["蓝牙", "UART3 RX", "PB3", "51", "输入 / 速率待定", tag("PB5 → PB3"), "MCU RX ← 蓝牙TX"],
    ["按键", "SW1", "PA27", "31", "输入 / 上拉 / 低有效", tag("保持", GREEN), "选择；按键另一端接GND"],
    ["按键", "SW2", "PA17", "10", "输入 / 上拉 / 低有效", tag("保持", GREEN), "确认；按键另一端接GND"],
    ["按键", "SW3", "PB23", "22", "输入 / 上拉 / 低有效", tag("新增"), "参数加；按键另一端接GND"],
    ["按键", "SW4", "PB27", "29", "输入 / 上拉 / 低有效", tag("新增"), "参数减；按键另一端接GND"],
    ["蜂鸣器", "BUZZER", "PB12", "64", "输出 / 低有效", tag("PB8 → PB12"), "建议三极管驱动；负载勿直挂 GPIO"],
    ["指示灯", "LIGHT", "PB13", "1", "输出 / 高有效", tag("PB9 → PB13"), "LED 串限流电阻或用三极管"],
    ["DCC云台PAN", "PWM1 / STEP", "PA26", "30", "输出 / 诊断100 Hz", tag("保留现有STEP", GREEN), "串33-100Ω；对地10k下拉"],
    ["DCC云台PAN", "DIR1", "PA14", "7", "输出", tag("保留现有DIR", GREEN), "串33-100Ω；对地10k下拉"],
    ["DCC云台TILT", "PWM2 / STEP", "PB4", "52", "输出 / 诊断100 Hz", tag("新增"), "蓝牙已迁PB2；串33-100Ω；10k下拉"],
    ["DCC云台TILT", "DIR2", "PB5", "53", "输出", tag("新增"), "蓝牙已迁PB3；串33-100Ω；10k下拉"],
    ["DCC云台", "SLP1+SLP2 / AWAKE", "PB17", "14", "输出 / 高工作低休眠", tag("新增"), "两路SLP并接；PB17对地10k下拉"],
    ["XDS110", "SWDIO", "PA19", "12", "双向调试", tag("板载保留"), "接XDS110 SWDIO；不作普通GPIO"],
    ["XDS110", "SWCLK", "PA20", "13", "调试时钟", tag("板载保留"), "接XDS110 SWCLK；不作普通GPIO"],
    ["XDS110", "NRST", "NRST", "38", "复位", tag("板载保留"), "接XDS110 RESET"],
]

changed_pin_row_indices = {1, 4, 7, 13, 18, 20, 21, 24, 25, 26, 27, 30, 31, 34, 35}
row_bgs = {i: LIGHT_RED for i in changed_pin_row_indices}
story += [
    make_table(pin_rows, [19*mm, 27*mm, 17*mm, 13*mm, 34*mm, 28*mm, 44*mm], row_bgs=row_bgs),
    Spacer(1, 3 * mm),
    P("说明：PA0/PA1 具有特殊 5 V 容限结构，但同一排线中的其他循迹输入并不都具备该能力。为避免混用，整套外设统一按 3.3 V 电平设计。", "CNSmall"),
    PageBreak(),
]

# Page 3: connector by connector
story += [
    section_title("2. 连接器逐端接线定义（可直接录入原理图）"),
    P("连接器编号是本文建议命名；可沿用你的旧位号，但网络名和 MCU 引脚应与下表一致。", "CNSmall"),
    P("J1 · TB6612 电机驱动", "CNH2"),
]

tb_rows = [
    ["端子", "连接到", "端子", "连接到", "电气说明"],
    ["VCC", "3V3", "VM", "电机电源", "VCC 旁 100 nF；VM 旁 100 nF + 220-470 µF"],
    ["GND", "系统 GND", "STBY", "PB16（脚4）", "STBY 10 kΩ 下拉；禁止硬接 3V3"],
    ["PWMA", "PA29（脚36）", "PWMB", "PA30（脚37）", "均为已修改 PWM 引脚"],
    ["AIN1", "PB24（脚23）", "AIN2", "PB20（脚19）", "左轮方向"],
    ["BIN1", "PB19（脚16）", "BIN2", "PB18（脚15）", "右轮方向"],
    ["AO1/AO2", "左电机", "BO1/BO2", "右电机", "电机线远离编码器/UART/SWD"],
]
story += [make_table(tb_rows, [24*mm, 39*mm, 24*mm, 39*mm, 48*mm]), Spacer(1, 2*mm)]

track_rows = [
    ["J2 循迹端子", "D1/L4", "D2/L3", "D3/L2", "D4/L1", "D5/R1", "D6/R2", "D7/R3", "D8/R4", "电源"],
    ["MCU", "PA0", "PA25", "PA31", "PA28", "PA1", "PA7", tag("PB25"), "PA16", "3V3 + GND"],
    ["LQFP64", "33", "26", "39", "35", "34", "49", tag("27"), "9", "-"],
]
story += [P("J2 · 8 路数字循迹", "CNH2"), make_table(track_rows, [26*mm]+[15*mm]*8+[28*mm]), Spacer(1, 2*mm)]

enc_rows = [
    ["连接器", "端子 1", "端子 2", "端子 3", "端子 4", "备注"],
    ["J3 左编码器", "3V3", "GND", "A → PA12", "B → PA13", "输出类型未知：预留 4.7-10 kΩ 上拉（可 DNP）"],
    ["J4 右编码器", "3V3", "GND", "A → PA24", "B → PA15", "输出类型未知：预留 4.7-10 kΩ 上拉（可 DNP）"],
]
story += [P("J3/J4 · 左右编码器", "CNH2"), make_table(enc_rows, [29*mm, 25*mm, 25*mm, 29*mm, 29*mm, 37*mm]), Spacer(1, 2*mm)]

small_conn_rows = [
    ["连接器", "端子定义（从 1 脚起）", "注意"],
    ["J5 OLED", "1:3V3  2:GND  3:SCL→PB0  4:SDA→PB1", "与TOF共用I2C；SCL/SDA预留4.7k上拉"],
    ["J6 IMU", "1:3V3  2:GND  3:IMU_RX←PA8  4:IMU_TX→PA9", "UART1 9600；TX/RX 必须交叉"],
    ["J7 K230", "1:GND→庐山派9脚  2:PB15→庐山派13脚/RXD  3:PA22←庐山派11脚/TXD", "UART2 115200；三针接口；不得接物理12脚CTS、PA10/PA11或USB口"],
    ["J8 蓝牙", "1:3V3  2:GND  3:BT_RX←PB2  4:BT_TX→PB3", "UART3；模块波特率未确认，代码尚未启用"],
    ["J9 TOF200F", "1:VIN(3-5V) 2:GND 3:SDA→PB1 4:SCL→PB0 5:RX 6:TX", "按信号名连接；5/6仅留配置/恢复测试点"],
    ["J10 DCC控制(自定义)", "1:PWM1←PA26 2:DIR1←PA14 3:DCY1=NC 4:SLP1←PB17 5:RST1=10k→3V3 6:GND 7:PWM2←PB4 8:DIR2←PB5 9:DCY2=NC 10:SLP2←PB17 11:RST2=10k→3V3 12:GND", "这是载板自定义脚序，不代表DCC模块排针顺序；接模块时必须按PWM/DIR/DCY/SLP/RST/GND信号名逐一对应"],
    ["J12 DCC电源", "绿色接线端子：DCC_VM=11-26V；DCC_GND=电源负极", "严格按模块PCB丝印的+/-接线；无单独3V3模块供电脚；DCC_GND与系统共地"],
    ["J13 DCC电机", "第一路电机→驱动1插座；第二路电机→驱动2插座", "按DCC插座丝印和电机相线/中心抽头资料连接，不得按普通四线电机臆测脚序"],
    ["J11 人机", "SW1→PA27 SW2→PA17 SW3→PB23 SW4→PB27 BUZZ→PB12 LIGHT→PB13", "四个按键均接地；蜂鸣器低有效；灯高有效"],
]
story += [P("J5-J13 · 低速外设", "CNH2"), make_table(small_conn_rows, [28*mm, 88*mm, 58*mm])]

story += [PageBreak()]

# Page 4: modified nets as PCB ECO instructions
story += [
    section_title("3. 旧 PCB 改版操作单（ECO）"),
    box(
        "下面每一行都可以作为原理图/PCB 的一条 ECO。建议在旧工程中先按网络名全局搜索，再改 MCU 端；不要只改丝印。完成后用 ERC/DRC 确认旧焊盘没有残留短接。",
        LIGHT_BLUE, BLUE, "CNBox",
    ),
    Spacer(1, 4*mm),
]

eco_rows = [
    ["ECO", "旧网络处理", "新网络处理", "PCB 走线检查", "状态"],
    ["ECO-01 PWMA", "断开 PWMA-PB6", "PWMA 接 PA29", "PB6 是板载 Flash CS，旧线必须完全移除", tag("必改")],
    ["ECO-02 PWMB", "断开 PWMB-PB7", "PWMB 接 PA30", "PB7 是板载 SPI1 MISO，旧线必须完全移除", tag("必改")],
    ["ECO-03 D7", "断开 D7-PA2/PA22/PA3", "D7 接 PB25（U7_7/U9_7）", "PA3未在排针引出；固件已同步到PB25", tag("再次必改")],
    ["ECO-04 蜂鸣器", "断开 BUZZ-PB8", "BUZZ 接 PB12", "PB8 与板载 Flash/LCD 资源相关；控制极性仍为低有效", tag("必改")],
    ["ECO-05 指示灯", "断开 LIGHT-PB9", "LIGHT 接 PB13", "PB9 与板载 Flash/LCD 时钟相关；高有效", tag("必改")],
    ["ECO-06 步进", "断开 STEP-PA18", "STEP 接 PA26", "PA18 是 BSL；上电复位时必须保持低，禁止再外接步进信号", tag("必改")],
    ["ECO-07 I2C", "OLED断开PB2/PB3", "OLED+TOF接PB0/PB1", "两设备共总线；代码已改开漏；预留4.7k上拉", tag("必改")],
    ["ECO-08 STBY", "移除 STBY-3V3 硬连", "STBY 接 PB16；加 10 kΩ 下拉", "确保复位期间电机默认关闭", tag("必改")],
    ["ECO-09 D2", "若旧板 D2 接 PB15，断开", "D2 接 PA25", "这是旧接线资料与当前代码不一致项", tag("必查")],
    ["ECO-10 K230", "删除UART0/PA10/PA11/PB1旧接法", "K230_RX←PB15；K230_TX→PA22", "PA10/PA11只给CH340；PB1只给I2C", tag("必改")],
    ["ECO-11 DCC TILT", "旧PCB无第二轴或PB4/PB5曾给蓝牙", "PWM2接PB4；DIR2接PB5", "蓝牙必须先迁移到PB2/PB3，禁止并接", tag("新增")],
    ["ECO-12 DCC SLP", "旧PCB无休眠控制或SLP硬接3V3", "SLP1、SLP2并接PB17；加10k下拉", "故障、任务结束和待机时由PB17拉低休眠", tag("新增")],
    ["ECO-13 限位", "不安装机械限位", "不分配限位GPIO", "上电位置仅作相对零点；堵转/丢步后位置会漂移", tag("已确认")],
    ["ECO-11 蓝牙", "断开蓝牙PB4/PB5", "蓝牙RX←PB2；TX→PB3", "UART3速率按模块确认后再启用", tag("必改")],
    ["ECO-12 TOF", "断开临时UART PB4/PB5", "TOF SCL/SDA接PB0/PB1", "模块先切IIC模式；UART仅留恢复测试点", tag("必改")],
]
story += [make_table(eco_rows, [23*mm, 39*mm, 40*mm, 55*mm, 24*mm], row_bgs={i: LIGHT_RED for i in range(1, len(eco_rows))})]

story += [
    Spacer(1, 4*mm),
    P("天猛星板载/保留引脚，不要从旧 PCB 继续占用", "CNH2"),
]
reserved_rows = [
    ["引脚", "板载/特殊用途", "载板处理"],
    ["PA5 / PA6", "板载 40 MHz HFXT 晶振", "不外接，不走高速/负载网络"],
    ["PA10 / PA11", "板载 CH340E UART0", "PA10 可一发多收；PA11 禁止两个 TX 直接并联"],
    ["PA18", "BSL 启动脚", "上电保持低；不接步进、按键或会拉高的外设"],
    ["PA19 / PA20", "SWDIO / SWCLK", "仅用于调试座和必要测试点"],
    ["PA21 / PA23", "板载电容影响高速用途", "只考虑低速 GPIO；本项目不使用"],
    ["PB6-PB11、PB14、PB26", "板载 Flash/LCD 相关", "不再分配给旧 PCB 外设"],
    ["PB21 / PB22", "板载用户按键 / LED", "本项目可不用；避免无意并联外设"],
]
story += [make_table(reserved_rows, [38*mm, 68*mm, 68*mm])]

story += [PageBreak()]

# Page 5: serial allocation and SWD
story += [
    section_title("4. 串口最终分配与 XDS110 调试接口"),
    P("4个硬件UART一对一分配；TOF改走I2C", "CNH2"),
]

k230_rows = [
    ["端点", "外设", "MCU TX", "MCU RX", "速率", "状态"],
    ["电脑调试", "UART0", "PA10", "PA11", "115200", "板载CH340，保持"],
    ["IMU", "UART1", "PA8", "PA9", "9600", "已启用"],
    ["K230", "UART2", "PB15", "PA22", "115200", "代码已接入，默认关闭"],
    ["蓝牙", "UART3", "PB2", "PB3", "待确认", "仅预留，未初始化"],
    ["TOF200F", "软件I2C", "PB0/SCL", "PB1/SDA", "I2C", "与OLED共线，驱动待接入"],
]
story += [make_table(k230_rows, [27*mm, 25*mm, 29*mm, 29*mm, 28*mm, 43*mm], row_bgs={3: LIGHT_GREEN, 4: LIGHT_ORANGE, 5: LIGHT_ORANGE})]
story += [
    Spacer(1, 3*mm),
    box(
        "<b>K230方向：</b>庐山派物理11脚/GPIO5/UART2_TXD → PA22/UART2_RX；PB15/UART2_TX → 庐山派物理13脚/GPIO6/UART2_RXD；物理9脚GND共地。物理12脚是CTS，不是RXD。<br/><b>USB警告：</b>庐山派USB-A不能直接插天猛星Type-C代替TTL串口；正式通信必须走9/11/13三针接口。所有外设与MSPM0共地并使用3.3V TTL。",
        LIGHT_RED, RED, "CNBox",
    ),
    Spacer(1, 5*mm),
    P("J12 · XDS110 最小 SWD 调试座", "CNH2"),
]

swd_rows = [
    ["信号名", "天猛星/MCU", "连接 XDS110", "PCB 规则"],
    ["VTref", "目标板 3V3", "VTref", "只作为目标电压参考；除非明确设计，否则不要由探针反向供电"],
    ["GND", "系统 GND", "GND", "至少 1 根，靠近 SWCLK 回流"],
    ["SWDIO", "PA19 · 脚12", "SWDIO", "短、少过孔；可预留 22-47 Ω 串阻位"],
    ["SWCLK", "PA20 · 脚13", "SWCLK", "短、少过孔；远离电机 PWM"],
    ["nRESET", "NRST · 脚38", "nRESET", "建议保留；方便可靠连接和恢复"],
]
story += [make_table(swd_rows, [32*mm, 42*mm, 35*mm, 65*mm])]
story += [
    Spacer(1, 3*mm),
    P("推荐放置带方向标识的5针或标准Cortex 10针接口；XDS110与J-Link都使用同一组SWDIO/SWCLK/NRST/VTref/GND信号，但连接器脚序必须按探针官方定义。", "CNSmall"),
    P("DCC-100v3 双轴控制与默认状态", "CNH2"),
    box(
        "DCC-100v3资料确认逻辑输入兼容3.3 V，模块只通过绿色端子使用11-26 V供电，没有单独的3.3 V模块供电脚。PAN使用PA26/PA14，TILT使用PB4/PB5；两路SLP并接PB17并加10 kΩ下拉。RST1/2分别以10 kΩ上拉到3.3 V，DCY1/2悬空使用自动衰减。诊断固件每次按键只发8个脉冲，STEP高/低各5 ms，结束后PB17立即拉低。机构无机械限位，禁止把相对脉冲数当作绝对位置。",
        LIGHT_BLUE, BLUE, "CNBox",
    ),
    PageBreak(),
]

# Page 6: power and release checklist
story += [
    section_title("5. 供电、地、保护与 PCB 冻结检查"),
    P("建议电源域", "CNH2"),
]

power_rows = [
    ["电源域", "供给对象", "连接原则", "去耦/保护建议"],
    ["3V3_LOGIC", "天猛星 I/O、OLED、IMU、3.3 V 编码器/循迹", "只给 3.3 V 兼容外设；严禁 5 V 输出直入普通 GPIO", "每个接口旁 100 nF；外接线可加 ESD/串阻"],
    ["VM_MOTOR", "TB6612 VM 与直流电机", "独立大电流路径；与逻辑共地但不要共用细长回流", "TB6612 旁 220-470 µF + 100 nF；入口反接/TVS 按电池选"],
    ["V_K230", "K230 开发板", "按 K230 板规定输入电压和峰值电流供电；不可从 MCU 3V3 硬拉", "大容量输入电容；避免断电时经 UART 反向供电"],
    ["DCC_VM", "DCC-100v3与两轴步进电机", "11-26 V；不得连接MCU 3V3/5V；逻辑地与系统共地", "DCC入口附近大电解+100nF；按电机电流选线宽与连接器"],
    ["GND", "全部模块", "逻辑、K230、TB6612、DCC、XDS110 必须共参考地", "优先完整地平面；电机回流不穿过 MCU/编码器区域"],
]
story += [make_table(power_rows, [29*mm, 38*mm, 63*mm, 51*mm])]

story += [
    Spacer(1, 4*mm),
    P("下单前 4 项冻结条件", "CNH2"),
]

freeze_rows = [
    ["序号", "必须完成", "通过判据", "当前状态"],
    ["F1", "PCB ECO复核", "D7=PB25；OLED/TOF=PB0/PB1；蓝牙=PB2/PB3；K230=PB15/PA22；DCC=PA26/PA14/PB4/PB5/PB17", tag("待人工复核")],
    ["F2", "CCS编译 + XDS110烧录", "D7改为PB25后已编译；目标板回来后稳定下载运行", tag("编译通过/烧录待测")],
    ["F3", "3.3 V电平核对", "循迹、编码器、IMU、OLED、TOF、蓝牙、K230实测不超VDD+0.3V", tag("未完成")],
    ["F4", "样机波形验证", "PWM、编码器、DCC双轴STEP/DIR/SLP、4路UART及I2C波形和功能均通过", tag("未完成")],
]
story += [make_table(freeze_rows, [18*mm, 46*mm, 91*mm, 26*mm], row_bgs={i: LIGHT_ORANGE for i in range(1, 5)})]

story += [
    Spacer(1, 4*mm),
    P("原理图/ERC 终检清单", "CNH2"),
    P("□ 所有 M1-M11 红色修改项已落实；□ D7最终接PB25的U7_7/U9_7而非PA22/PA3；□ 固件D7已同步为GPIOB/PIN25/PB25并重新编译；□ PA10/PA11只给CH340；□ PB0/PB1只作OLED+TOF I2C；□ PB2/PB3只给蓝牙；□ PB15/PA22只给K230；□ STBY不再硬接3V3；□ D2接PA25；□ PA18上电保持低；□ PA19/PA20/NRST调试接口完整；□ 每个外设连接器有GND；□ 所有外接输入为3.3V；□ 天猛星排针封装方向和1脚标记按官方机械资料核对。", "CNBody"),
    Spacer(1, 4*mm),
    P("6. 依据与可追溯性", "CNH1"),
    P("代码核对文件：car_final_ccs/ml_libs/ml_pwm.c、ml_motor.c、ml_uart.c、ml_i2c.h、ml_oled.h；car_final_ccs/user/xunji.c、encoder.c、key.c、buzzer_light.c、main.c、IMU.c。", "CNSmall"),
    P("硬件资料：TI MSPM0G3507 数据手册（封装脚号、电气限制）；天猛星板载资源资料；TOF200F V2.0.1规格书；本工作区K230-CanMV开发板资料。", "CNSmall"),
    P("在线来源：<link href='https://www.ti.com/lit/ds/symlink/mspm0g3507.pdf' color='#276FBF'>TI MSPM0G3507 datasheet</link>；<link href='https://wiki.lckfb.com/zh-hans/tmx-mspm0g3507/' color='#276FBF'>天猛星介绍</link>；<link href='https://wiki.lckfb.com/zh-hans/tmx-mspm0g3507/download-center.html' color='#276FBF'>天猛星下载中心</link>；<link href='https://wiki.lckfb.com/zh-hans/tmx-mspm0g3507/ccs-beginner/uart.html' color='#276FBF'>天猛星 UART0/CH340 说明</link>。", "CNTiny"),
    Spacer(1, 3*mm),
    box(
        "<b>结论：</b> v4.0是当前完整接线基线，v2/v3均已过时。DCC-100v3双轴采用PAN=PA26/PA14、TILT=PB4/PB5、SLP=PB17，机械限位明确不安装。诊断固件已编译，但本文件仍不是生产放行文件；完成红色ECO、XDS110烧录和DCC/串口/I2C波形验证后再正式放行。",
        LIGHT_GREEN, GREEN, "CNBox",
    ),
]

doc.build(story)
print(OUT)
