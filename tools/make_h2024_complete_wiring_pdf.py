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
OUT = ROOT / "output" / "pdf" / "H2024_MSPM0G3507_25E_PCB_wiring_v1_1.pdf"
OUT.parent.mkdir(parents=True, exist_ok=True)

pdfmetrics.registerFont(TTFont("CN", r"C:\Windows\Fonts\msyh.ttc"))
pdfmetrics.registerFont(TTFont("CN-Bold", r"C:\Windows\Fonts\msyhbd.ttc"))

PAGE_W, PAGE_H = A4
ML = 14 * mm
MR = 14 * mm
MT = 17 * mm
MB = 15 * mm
CONTENT_W = PAGE_W - ML - MR

NAVY = colors.HexColor("#17324D")
BLUE = colors.HexColor("#2563A6")
GREEN = colors.HexColor("#1F7A4D")
RED = colors.HexColor("#C62828")
ORANGE = colors.HexColor("#D86800")
GRAY = colors.HexColor("#5E6872")
GRID = colors.HexColor("#AEB8C2")
LIGHT_BLUE = colors.HexColor("#EAF3FB")
LIGHT_GREEN = colors.HexColor("#EAF6EF")
LIGHT_RED = colors.HexColor("#FDECEC")
LIGHT_ORANGE = colors.HexColor("#FFF2E5")
LIGHT_GRAY = colors.HexColor("#F3F5F7")

styles = getSampleStyleSheet()
styles.add(ParagraphStyle(
    name="BodyCN", fontName="CN", fontSize=8.5, leading=12.5,
    textColor=colors.HexColor("#20262C"), spaceAfter=4,
))
styles.add(ParagraphStyle(
    name="SmallCN", parent=styles["BodyCN"], fontSize=7.3, leading=10.4,
))
styles.add(ParagraphStyle(
    name="TinyCN", parent=styles["BodyCN"], fontSize=6.4, leading=8.5,
))
styles.add(ParagraphStyle(
    name="TitleCN", fontName="CN-Bold", fontSize=22, leading=30,
    alignment=TA_CENTER, textColor=NAVY, spaceAfter=6,
))
styles.add(ParagraphStyle(
    name="SubtitleCN", fontName="CN", fontSize=10.5, leading=15,
    alignment=TA_CENTER, textColor=GRAY, spaceAfter=10,
))
styles.add(ParagraphStyle(
    name="H1CN", fontName="CN-Bold", fontSize=14, leading=19,
    textColor=NAVY, spaceBefore=3, spaceAfter=7,
))
styles.add(ParagraphStyle(
    name="H2CN", fontName="CN-Bold", fontSize=10.5, leading=14,
    textColor=BLUE, spaceBefore=4, spaceAfter=4,
))
styles.add(ParagraphStyle(
    name="BoxCN", fontName="CN", fontSize=8.2, leading=12,
    textColor=colors.HexColor("#20262C"), alignment=TA_LEFT,
))
styles.add(ParagraphStyle(
    name="CellCN", fontName="CN", fontSize=6.7, leading=8.6,
    textColor=colors.HexColor("#20262C"),
))
styles.add(ParagraphStyle(
    name="CellHeadCN", parent=styles["CellCN"], fontName="CN-Bold",
    textColor=colors.white,
))


def P(text, style="BodyCN"):
    return Paragraph(str(text), styles[style])


def header_footer(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(colors.HexColor("#D5DCE2"))
    canvas.setLineWidth(0.45)
    canvas.line(ML, PAGE_H - 11.5 * mm, PAGE_W - MR, PAGE_H - 11.5 * mm)
    canvas.line(ML, 10.2 * mm, PAGE_W - MR, 10.2 * mm)
    canvas.setFont("CN", 6.8)
    canvas.setFillColor(GRAY)
    canvas.drawString(ML, PAGE_H - 9.1 * mm, "2024 H题自动行驶小车 · MSPM0G3507完整接线表")
    canvas.drawRightString(PAGE_W - MR, PAGE_H - 9.1 * mm, "v1.1 · 2026-07-18")
    canvas.drawString(ML, 6.5 * mm, "依据：car_h2024_ccs当前源码；引脚方向均从MSPM0视角描述")
    canvas.drawRightString(PAGE_W - MR, 6.5 * mm, f"第 {doc.page} 页")
    canvas.restoreState()


frame = Frame(
    ML, MB, CONTENT_W, PAGE_H - MT - MB,
    leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0,
)
doc = BaseDocTemplate(
    str(OUT), pagesize=A4,
    leftMargin=ML, rightMargin=MR, topMargin=MT, bottomMargin=MB,
    title="2024 H题自动行驶小车 MSPM0G3507 完整接线表",
    author="Codex",
)
doc.addPageTemplates([PageTemplate(id="main", frames=[frame], onPage=header_footer)])


def box(text, bg=LIGHT_BLUE, border=BLUE, pad=7, style="BoxCN"):
    table = Table([[P(text, style)]], colWidths=[CONTENT_W])
    table.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), bg),
        ("BOX", (0, 0), (-1, -1), 0.8, border),
        ("LEFTPADDING", (0, 0), (-1, -1), pad),
        ("RIGHTPADDING", (0, 0), (-1, -1), pad),
        ("TOPPADDING", (0, 0), (-1, -1), pad),
        ("BOTTOMPADDING", (0, 0), (-1, -1), pad),
    ]))
    return table


def make_table(rows, widths, header=True, tiny=False, row_bgs=None, repeat=1):
    cooked = []
    for row_idx, row in enumerate(rows):
        cooked.append([
            cell if isinstance(cell, (Paragraph, Table)) else P(
                cell,
                "CellHeadCN" if header and row_idx == 0 else ("TinyCN" if tiny else "CellCN"),
            )
            for cell in row
        ])
    table = LongTable(cooked, colWidths=widths, repeatRows=repeat if header else 0, hAlign="LEFT")
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
        ]
        start = 1
    else:
        start = 0
    for i in range(start, len(rows)):
        commands.append(("BACKGROUND", (0, i), (-1, i), colors.white if i % 2 else LIGHT_GRAY))
    if row_bgs:
        for idx, color in row_bgs.items():
            commands.append(("BACKGROUND", (0, idx), (-1, idx), color))
    table.setStyle(TableStyle(commands))
    return table


story = []

# Page 1 - quick wiring overview
story += [
    Spacer(1, 8 * mm),
    P("2024 H题 自动行驶小车", "TitleCN"),
    P("天猛星 MSPM0G3507 · 完整接线表", "TitleCN"),
    P("对应 car_h2024_ccs / APP_PROFILE_H2024=1 / 不含25E扩展机构", "SubtitleCN"),
    box(
        "<b>文档状态：源码核对与TI-Clang编译完成，实车逐线验证尚未完成。</b> 本表按用户现用25E PCB裁剪：OLED、JY61P、MODE/SET两按键、TB6612、电机编码器、8路循迹、蜂鸣器/指示灯和J-Link SWD。"
        "K230、蓝牙、TOF、步进云台、激光当前均不初始化。",
        LIGHT_ORANGE, ORANGE,
    ),
    Spacer(1, 5 * mm),
    P("一页速查", "H1CN"),
]

quick_rows = [
    ["模块", "MSPM0信号", "外设端子", "供电/电平", "当前用途"],
    ["TB6612+左右电机", "PA29/PB24/PB20；PA30/PB19/PB18；PB16", "PWMA/AIN1/AIN2；PWMB/BIN1/BIN2；STBY", "逻辑3.3V；VM独立", "任务运动控制"],
    ["左右编码器", "PA12/PA13；PA24/PA15", "L-A/L-B；R-A/R-B", "输出必须≤3.3V", "速度/里程采样"],
    ["JY61P IMU", "PA8 TX；PA9 RX", "RX；TX", "9600, 8N1, 3.3V TTL", "任务1航向保持"],
    ["0.96寸OLED", "PB2 SCL；PB3 SDA", "SCL；SDA", "3.3V，I2C地址0x3C", "任务/状态显示"],
    ["8路循迹", "PA0/PA25/PA31/PA28/PA1/PA7/PA22/PA16", "D1...D8", "3.3V；黑线低电平", "任务2-4路径识别"],
    ["两按键", "PA17 MODE；PA27 SET", "MODE；SET/START", "内部上拉；按下接GND", "选择/启动；运行时任一键停车"],
    ["声光", "PB12；PB13", "BUZZER_CTRL；LIGHT_CTRL", "PB12低有效；PB13高有效", "到点/停车提示"],
    ["J-Link SWD", "PA19；PA20；NRST", "SWDIO；SWCLK；RESET", "VTref=3.3V；必须共地", "下载/调试"],
]
story.append(make_table(quick_rows, [29*mm, 58*mm, 43*mm, 27*mm, 25*mm]))
story += [
    Spacer(1, 5 * mm),
    P("供电关系", "H2CN"),
]

flow = Table([
    [P("<b>电池/总开关/保险</b>", "BoxCN"), P("→", "H1CN"), P("<b>电机电源支路</b><br/>TB6612 VM → 左右直流电机", "BoxCN")],
    ["", P("→", "H1CN"), P("<b>稳压逻辑支路</b><br/>天猛星、TB6612 VCC、传感器、OLED、IMU", "BoxCN")],
    [P("<b>所有模块GND</b>", "BoxCN"), P("→", "H1CN"), P("<b>公共参考地</b><br/>电机大电流回路与MCU小信号回路分开走线后汇接", "BoxCN")],
], colWidths=[50*mm, 12*mm, 120*mm])
flow.setStyle(TableStyle([
    ("BACKGROUND", (0, 0), (0, 0), LIGHT_ORANGE),
    ("BACKGROUND", (0, 2), (0, 2), LIGHT_GREEN),
    ("BACKGROUND", (2, 0), (2, 2), LIGHT_BLUE),
    ("BOX", (0, 0), (0, 0), 0.8, ORANGE),
    ("BOX", (0, 2), (0, 2), 0.8, GREEN),
    ("BOX", (2, 0), (2, 2), 0.8, BLUE),
    ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
    ("ALIGN", (1, 0), (1, -1), "CENTER"),
    ("LEFTPADDING", (0, 0), (-1, -1), 6),
    ("RIGHTPADDING", (0, 0), (-1, -1), 6),
    ("TOPPADDING", (0, 0), (-1, -1), 7),
    ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
]))
story += [flow, Spacer(1, 5 * mm)]
story.append(box(
    "<b>三条硬规则：</b> ① 普通GPIO不得直接接5V信号；② 电机不得由开发板3V3/5V供电；③ 所有模块必须共地，但电机电流回流不要穿过MCU/IMU/OLED地线。",
    LIGHT_RED, RED,
))
story.append(PageBreak())

# Page 2 - motor and encoders
story += [P("1. 电机驱动与编码器接线", "H1CN")]
motor_rows = [
    ["模块端", "连接到MSPM0/电源", "LQFP64", "方向/参数", "说明"],
    ["TB6612 VCC", "3V3_LOGIC", "-", "逻辑电源", "就近100nF去耦"],
    ["TB6612 VM", "VM_MOTOR", "-", "电机电源", "按电机额定电压选择；入口220-470µF+100nF"],
    ["TB6612 GND", "公共GND", "-", "参考地", "与MSPM0共地；电机回流独立走线"],
    ["PWMA", "PA29 / TIMG6_C0", "36", "MCU输出；1kHz PWM", "左电机速度"],
    ["AIN1", "PB24", "23", "MCU输出", "左电机方向1"],
    ["AIN2", "PB20", "19", "MCU输出", "左电机方向2"],
    ["AO1/AO2", "左直流电机两端", "-", "大电流输出", "悬空轮测试；方向反可交换两根电机线"],
    ["PWMB", "PA30 / TIMG6_C1", "37", "MCU输出；1kHz PWM", "右电机速度"],
    ["BIN1", "PB19", "16", "MCU输出", "右电机方向1"],
    ["BIN2", "PB18", "15", "MCU输出", "右电机方向2"],
    ["BO1/BO2", "右直流电机两端", "-", "大电流输出", "悬空轮测试；方向反可交换两根电机线"],
    ["STBY", "PB16", "4", "高=工作，低=待机", "建议10kΩ下拉；不可硬接3.3V"],
]
story.append(make_table(motor_rows, [30*mm, 48*mm, 20*mm, 38*mm, 46*mm], row_bgs={i: LIGHT_RED for i in [4, 8, 12]}))
story += [
    Spacer(1, 4 * mm),
    box(
        "H2024固件上电时PB16保持低，电机驱动处于待机；收到有效IMU帧并按SET/START后才置高。运行中任一按键、IMU丢帧、完成或15秒超时都会立即把PWM清零并拉低PB16。",
        LIGHT_GREEN, GREEN,
    ),
    Spacer(1, 5 * mm),
    P("编码器", "H2CN"),
]
encoder_rows = [
    ["编码器端", "MSPM0", "LQFP64", "当前输入配置", "接线要求"],
    ["左编码器 A", "PA12", "5", "上升沿中断；内部上拉", "输出幅度≤3.3V"],
    ["左编码器 B", "PA13", "6", "下降沿中断；代码内下拉", "先确认是推挽还是开集电极"],
    ["右编码器 A", "PA24", "25", "上升沿中断；内部上拉", "输出幅度≤3.3V"],
    ["右编码器 B", "PA15", "8", "下降沿中断；代码内下拉", "先确认是推挽还是开集电极"],
    ["编码器 VCC", "按编码器规格供电", "-", "不得据线色猜电压", "若5V供电，输出必须经分压/电平转换到3.3V"],
    ["编码器 GND", "公共GND", "-", "信号参考地", "与MSPM0、TB6612共地"],
]
story.append(make_table(encoder_rows, [32*mm, 28*mm, 20*mm, 48*mm, 54*mm]))
story += [
    Spacer(1, 5 * mm),
    P("电机插头建议", "H2CN"),
    P("将左右电机动力线与编码器线分开做插头，避免维修时把VM误送入编码器输入。建议：动力2Pin；编码器VCC/GND/A/B独立4Pin，并在丝印上明确L/R与A/B。"),
    P("首次上电必须架空车轮，先确认MODE或SET任一键能让PB16关闭，再进行低PWM方向测试。不要把整车放地面后首次启动。"),
    PageBreak(),
]

# Page 3 - sensors
story += [P("2. IMU、OLED与8路循迹接线", "H1CN")]
imu_rows = [
    ["JY61P端", "MSPM0端", "LQFP64", "通信", "说明"],
    ["TX", "PA9 / UART1_RX", "55", "9600, 8N1", "任务1必须；交叉连接"],
    ["RX", "PA8 / UART1_TX", "54", "9600, 8N1", "用于配置/双向通信；交叉连接"],
    ["GND", "公共GND", "-", "参考地", "必须连接"],
    ["VCC", "按模块铭牌供电", "-", "模块电源", "无论供电多少，TX逻辑输出必须≤3.3V"],
]
story.append(make_table(imu_rows, [30*mm, 43*mm, 20*mm, 34*mm, 55*mm]))
story += [
    Spacer(1, 3 * mm),
    box("启动任务1前，OLED必须能看到IMU角度变化；没有收到航向帧时固件会显示 WAIT IMU DATA 并拒绝启动。运行中500ms无新帧会停车。", LIGHT_GREEN, GREEN),
    Spacer(1, 5 * mm),
    P("OLED（SSD1306，4Pin I2C）", "H2CN"),
]
oled_rows = [
    ["OLED端", "MSPM0端", "LQFP64", "电气要求", "备注"],
    ["VCC", "3V3_LOGIC", "-", "3.3V", "不要把5V逻辑接入PB2/PB3"],
    ["GND", "公共GND", "-", "参考地", "必须连接"],
    ["SCL", "PB2", "50", "软件I2C，开漏", "外部上拉到3.3V；建议4.7kΩ预留"],
    ["SDA", "PB3", "51", "软件I2C，开漏", "外部上拉到3.3V；建议4.7kΩ预留"],
]
story.append(make_table(oled_rows, [28*mm, 30*mm, 20*mm, 48*mm, 56*mm]))
story += [Spacer(1, 5 * mm), P("8路循迹传感器（从车头朝前看：左→右）", "H2CN")]
track_rows = [
    ["位置", "模块端", "MSPM0", "LQFP64", "电平", "备注"],
    ["最左", "D1 / L4", "PA0", "33", "白=高，黑=低", "内部上拉"],
    ["左3", "D2 / L3", "PA25", "26", "白=高，黑=低", "不要按旧表接PB15"],
    ["左2", "D3 / L2", "PA31", "39", "白=高，黑=低", "内部上拉"],
    ["左1", "D4 / L1", "PA28", "35", "白=高，黑=低", "靠近中心"],
    ["右1", "D5 / R1", "PA1", "34", "白=高，黑=低", "靠近中心"],
    ["右2", "D6 / R2", "PA7", "49", "白=高，黑=低", "内部上拉"],
    ["右3", "D7 / R3", "PA22", "18", "白=高，黑=低", "按25E PCB接PA22"],
    ["最右", "D8 / R4", "PA16", "9", "白=高，黑=低", "内部上拉"],
    ["供电", "VCC", "3V3_LOGIC", "-", "3.3V", "若模块只能5V供电，8路输出必须电平转换"],
    ["地", "GND", "公共GND", "-", "参考地", "必须连接"],
]
story.append(make_table(track_rows, [23*mm, 28*mm, 27*mm, 20*mm, 40*mm, 44*mm]))
story += [
    Spacer(1, 4 * mm),
    P("任务1当前只使用IMU直行，但8路循迹仍在上电时初始化；为任务2-4预先接好。传感器排布必须与D1最左、D8最右一致，否则后续循迹转向方向会反。", "SmallCN"),
    PageBreak(),
]

# Page 4 - controls, debug, disabled pins
story += [P("3. 按键、声光、调试与禁用接口", "H1CN")]
ui_rows = [
    ["功能", "MSPM0", "LQFP64", "另一端/负载", "有效电平", "H2024行为"],
    ["MODE", "PA17", "10", "按键另一端接GND", "按下=低", "待机选择任务；运行中停车"],
    ["SET/START", "PA27", "31", "按键另一端接GND", "按下=低", "待机启动任务；运行中停车"],
    ["蜂鸣器控制", "PB12", "64", "接三极管/MOS驱动级", "低有效", "完成/故障提示约500ms"],
    ["指示灯控制", "PB13", "1", "LED+限流或驱动级", "高有效", "与蜂鸣器同步提示"],
]
story.append(make_table(ui_rows, [31*mm, 25*mm, 20*mm, 52*mm, 27*mm, 27*mm], row_bgs={1: LIGHT_RED, 2: LIGHT_RED}))
story += [
    Spacer(1, 4 * mm),
    box("蜂鸣器和高亮LED不可直接以大电流挂在GPIO上。PB12/PB13应只驱动三极管或MOS管控制端；感性蜂鸣器需要按器件类型增加续流保护。", LIGHT_RED, RED),
    Spacer(1, 5 * mm),
    P("板载CH340与J-Link下载调试", "H2CN"),
]
debug_rows = [
    ["接口", "MCU引脚", "LQFP64", "连接", "用途/限制"],
    ["UART0 TX", "PA10", "56", "板载CH340 RX", "H2024不初始化；开发板内部物理连接保留"],
    ["UART0 RX", "PA11", "57", "板载CH340 TX", "H2024不初始化；不得再接K230 TX"],
    ["SWDIO", "PA19", "12", "J-Link SWDIO", "调试保留，不作普通GPIO"],
    ["SWCLK", "PA20", "13", "J-Link SWCLK", "调试保留，不作普通GPIO"],
    ["NRST", "NRST", "38", "J-Link RESET", "建议连接，便于可靠复位"],
]
story.append(make_table(debug_rows, [30*mm, 30*mm, 20*mm, 48*mm, 54*mm]))
story += [Spacer(1, 5 * mm), P("当前H2024未启用/不得混接", "H2CN")]
disabled_rows = [
    ["接口", "旧/预留引脚", "当前状态", "处理"],
    ["K230 UART0", "PA10 / PA11", "未初始化", "不接K230；PA10/PA11仅保留板载CH340物理连接"],
    ["K230 UART2", "PB15 TX / PA22 RX", "禁止启用", "PA22已用于循迹D7，不能再接K230 TX"],
    ["蓝牙 UART3", "PB2 TX / PB3 RX", "禁止启用", "PB2/PB3已用于OLED"],
    ["TOF200F", "PB0/PB1", "未初始化", "H题不用；保持接口断开"],
    ["步进云台", "PA26/PA14/PB4/PB5/PB17", "未初始化", "保持悬空，不接DCC驱动"],
    ["多余按键", "PB23 / PB27", "未初始化", "25E PCB H题固件只使用PA17/PA27"],
    ["旧STEP", "PA18", "废弃旧接法", "不要按旧PDF接线"],
    ["激光", "无已确认GPIO", "禁止", "H题禁止摄像头且当前固件无激光功能"],
]
story.append(make_table(disabled_rows, [36*mm, 49*mm, 37*mm, 60*mm], row_bgs={i: LIGHT_RED for i in range(1, len(disabled_rows))}))
story += [
    Spacer(1, 5 * mm),
    box("24H题当前固件没有摄像头路径。不要为了沿用25E线束而给K230、TOF、步进或激光上电；这会增加电源噪声、占用引脚并制造调试歧义。", LIGHT_ORANGE, ORANGE),
    PageBreak(),
]

# Page 5 - power and grounding
story += [P("4. 电源、地线、保护与线束", "H1CN")]
power_rows = [
    ["电源域", "供电对象", "连接原则", "必须检查"],
    ["VM_MOTOR", "TB6612 VM、左右直流电机", "从电池/电机稳压支路独立供电；粗线；不要经过开发板", "电压符合电机与TB6612规格；极性正确"],
    ["3V3_LOGIC", "TB6612 VCC、OLED、循迹数字接口、编码器逻辑、MCU外设", "由可靠3.3V稳压提供；每模块就近100nF", "总电流能力与纹波；任何GPIO不得被5V反灌"],
    ["IMU_VCC", "JY61P", "按具体模块铭牌供电；信号电平仍按3.3V TTL", "先测IMU TX高电平≤3.3V"],
    ["MCU供电", "天猛星开发板", "按开发板规定从USB或VIN/5V/3V3选择一种合规入口", "不得同时从多个电源口反向互灌"],
    ["GND", "全部模块", "必须共地；电机大电流与逻辑小信号分支布线后在电源入口附近汇接", "万用表确认各GND低阻连通"],
]
story.append(make_table(power_rows, [31*mm, 43*mm, 69*mm, 39*mm]))
story += [Spacer(1, 5 * mm), P("推荐保护/去耦", "H2CN")]
protect_rows = [
    ["位置", "建议器件", "作用"],
    ["电池入口", "总开关+保险丝/自恢复保险+反接保护", "防止短路和反接损坏"],
    ["TB6612 VM入口", "220-470µF电解+100nF陶瓷", "吸收电机启动/换向电流脉冲"],
    ["TB6612 VCC", "100nF陶瓷，就近", "逻辑去耦"],
    ["每个传感器插座", "100nF；必要时ESD/串阻", "降低插拔和长线干扰"],
    ["I2C SCL/SDA", "各4.7kΩ上拉到3.3V（若模块已带可DNP）", "保证开漏总线高电平"],
    ["STBY PB16", "10kΩ下拉", "复位/上电期间保持电机关闭"],
    ["蜂鸣器/灯", "三极管或MOS驱动；LED限流；感性负载保护", "避免GPIO过流"],
]
story.append(make_table(protect_rows, [45*mm, 78*mm, 59*mm]))
story += [Spacer(1, 5 * mm), P("线束与布线", "H2CN")]
wire_rows = [
    ["类别", "布线要求"],
    ["电机线", "双绞、短、远离IMU/UART/I2C/编码器；左右电机分别成束"],
    ["编码器线", "VCC/GND/A/B同束，长线可将每路信号与GND配对；不要与电机线平行长距离走线"],
    ["I2C线", "SCL/SDA与GND靠近，尽量短；避免与PWM和电机线同束"],
    ["IMU线", "TX/RX/GND成束，IMU远离电机和大电流线；机械固定避免振动"],
    ["接插件", "电机动力、逻辑电源、编码器、传感器使用防呆且不同针数/键位，丝印标出电压与方向"],
]
story.append(make_table(wire_rows, [39*mm, 143*mm]))
story += [
    Spacer(1, 5 * mm),
    box("任何模块供电不确定时：先断开信号线，查模块型号/原理图，用限流电源单独上电并测量输出高电平，再连接MSPM0。不要根据杜邦线颜色推断VCC/GND。", LIGHT_RED, RED),
    PageBreak(),
]

# Page 6 - bring-up checklist and traceability
story += [P("5. 上电检查与分阶段验收", "H1CN")]
check_rows = [
    ["阶段", "操作", "通过标准"],
    ["断电检查", "确认VM、3V3、GND无短路；核对TB6612 VCC/VM未接反；所有GPIO无5V直入", "万用表电阻/通断正常"],
    ["只上逻辑电", "断开电机VM，仅给MCU和传感器供电", "无异常发热；3V3稳定；OLED点亮"],
    ["IMU", "转动车体观察OLED yaw；保持静止检查帧计数持续更新", "SET不再提示WAIT IMU DATA"],
    ["按键/停车", "待机测试MODE选择、SET启动；运行中分别按两键", "PA17/PA27任一按键都能关闭PWM与PB16"],
    ["声光", "触发一次完成/故障提示", "蜂鸣器与灯约500ms后自动关闭"],
    ["循迹", "白底与黑线逐路覆盖D1-D8", "原始电气状态白=高、黑=低；左右顺序正确"],
    ["编码器", "手转左/右轮并查看计数/串口诊断", "每个轮子都有稳定计数，无悬空乱跳"],
    ["电机悬空", "架空车轮、限流电源、低PWM测试左右前进", "两轮前进方向一致；任一按键和断IMU均能停"],
    ["落地低速", "空旷场地、随时可断总电源", "无失控、无复位、无OLED/IMU掉线"],
    ["任务1", "A点摆正，按SET启动，测A→B距离与时间", "15秒内停车；声光提示；再标定PWM与700 ticks"],
]
story.append(make_table(check_rows, [31*mm, 93*mm, 58*mm]))
story += [Spacer(1, 5 * mm), P("常见故障快速定位", "H2CN")]
fault_rows = [
    ["现象", "优先检查"],
    ["OLED不亮", "VCC/GND、PB2/PB3是否接反、I2C上拉、地址0x3C"],
    ["WAIT IMU DATA", "IMU TX→PA9、共地、9600 8N1、IMU是否输出0x55 0x53帧"],
    ["电机不转", "PB16/STBY、VM、电机线、PA29/PA30 PWM、TB6612 VCC/GND"],
    ["一启动就复位", "电机VM与逻辑电源隔离、VM大电容、地线回流、供电压降"],
    ["编码器无计数", "A/B/VCC/GND、输出电平、PA12/PA13/PA24/PA15、上拉方式"],
    ["循迹左右反", "D1是否物理最左、D8是否最右；D7必须接PA22"],
    ["按键不能停车", "PA17/PA27是否按下接GND；先切断电机VM并检查线束/按键"],
]
story.append(make_table(fault_rows, [48*mm, 134*mm]))
story += [Spacer(1, 5 * mm), P("源码追溯", "H2CN")]
story.append(P(
    "主入口：car_h2024_ccs/user/main.c、h2024_app.c；电机：ml_libs/ml_motor.c、ml_pwm.c；编码器：user/encoder.c；IMU：user/IMU.c、ml_libs/ml_uart.c；循迹：user/xunji.c；OLED：ml_libs/ml_oled.c、ml_oled.h；按键：user/key.c；声光：user/buzzer_light.c；生成配置仅用于宏名核对：ti_msp_dl_config.h。",
    "SmallCN",
))
story += [Spacer(1, 5 * mm)]
story.append(box(
    "<b>结论：</b> 本PDF是H2024当前固件的完整接线基线。可以据此制作线束和逐项通电，但在完成第6页验收前，不应把整车直接放地面高速运行。任何实物模块型号或供电规格与本文假设不一致时，以模块原理图/数据手册为准并先停电复核。",
    LIGHT_GREEN, GREEN,
))

doc.build(story)
print(OUT)
