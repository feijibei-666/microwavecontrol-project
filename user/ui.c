#include "ui.h"
#include "lcd.h"

#define UI_WIDTH       128
#define UI_HEIGHT      160
#define BAND_HEIGHT    8
#define BAND_COUNT     (UI_HEIGHT / BAND_HEIGHT)
#define ALL_BANDS      ((1UL << BAND_COUNT) - 1)
#define RGB(r,g,b)     ((uint16_t)((((r) & 248) << 8) | (((g) & 252) << 3) | ((b) >> 3)))
#define C_BG           RGB(7, 15, 27)
#define C_CARD         RGB(16, 32, 49)
#define C_BORDER       RGB(29, 52, 73)
#define C_TEXT         RGB(237, 247, 252)
#define C_MUTED        RGB(123, 153, 175)
#define C_ACCENT       RGB(60, 220, 231)
#define C_BLUE         RGB(36, 122, 224)
#define C_GREEN        RGB(112, 235, 170)
#define C_AMBER        RGB(255, 199, 111)

/* 2 KiB instead of a 40 KiB full-screen framebuffer. Host-order RGB565. */
static uint16_t stripe[UI_WIDTH * BAND_HEIGHT];
static int16_t stripe_top;
static AppSnapshot view;
static uint8_t has_snapshot;
static uint8_t animating;
static uint8_t animation_band;
static int8_t previous_front;
static uint8_t direction_back;
static uint32_t dirty_bands;
static uint32_t last_render;
volatile uint32_t ui_rendered_bands;
volatile uint32_t ui_transition_count;

/* Compact 5x7 ASCII alphabet. Lowercase is intentionally rendered uppercase. */
static const uint8_t digit_font[10][5] = {
    {0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
    {0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1e}
};
static const uint8_t letter_font[26][5] = {
    {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},
    {0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},
    {0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x09,0x01},
    {0x3e,0x41,0x49,0x49,0x7a},{0x7f,0x08,0x08,0x08,0x7f},
    {0x00,0x41,0x7f,0x41,0x00},{0x20,0x40,0x41,0x3f,0x01},
    {0x7f,0x08,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
    {0x7f,0x02,0x0c,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},
    {0x3e,0x41,0x41,0x41,0x3e},{0x7f,0x09,0x09,0x09,0x06},
    {0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7f,0x01,0x01},
    {0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},
    {0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
};

static void Rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    int16_t xx, yy;
    int16_t x0 = (x < 0) ? 0 : x;
    int16_t x1 = (x + w > UI_WIDTH) ? UI_WIDTH : x + w;
    int16_t y0 = (y < stripe_top) ? stripe_top : y;
    int16_t y1 = (y + h > stripe_top + BAND_HEIGHT) ? stripe_top + BAND_HEIGHT : y + h;

    for (yy = y0; yy < y1; yy++)
    {
        for (xx = x0; xx < x1; xx++)
        {
            stripe[(yy - stripe_top) * UI_WIDTH + xx] = color;
        }
    }
}

static void RoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                      int16_t radius, uint16_t color)
{
    int16_t yy, dy, inset;
    int16_t y0 = (y < stripe_top) ? stripe_top : y;
    int16_t y1 = (y + h > stripe_top + BAND_HEIGHT) ? stripe_top + BAND_HEIGHT : y + h;

    for (yy = y0; yy < y1; yy++)
    {
        dy = yy - y;
        if (dy >= h - radius) dy = h - 1 - dy;
        inset = 0;
        if (dy < radius)
        {
            /* Stepped corners suit the physical pixel grid without floating point. */
            inset = (dy == 0) ? radius - 2 : (dy == 1) ? radius - 4 : (dy == 2) ? 1 : 0;
            if (inset < 0) inset = 0;
        }
        Rect(x + inset, yy, w - inset * 2, 1, color);
    }
}

static void Card(int16_t y, int16_t h)
{
    RoundRect(6, y, 116, h, 7, C_BORDER);
    RoundRect(7, y + 1, 114, h - 2, 6, C_CARD);
}

static uint8_t Glyph(char ch, uint8_t col)
{
    if ((ch >= 'a') && (ch <= 'z')) ch = (char)(ch - 'a' + 'A');
    if ((ch >= '0') && (ch <= '9')) return digit_font[ch - '0'][col];
    if ((ch >= 'A') && (ch <= 'Z')) return letter_font[ch - 'A'][col];
    switch (ch)
    {
        case ':': return (col == 2) ? 0x24 : 0;
        case '/': return (uint8_t)(0x40 >> col);
        case '-': return 0x08;
        case '+': return (col == 2) ? 0x3e : 0x08;
        case '>': return (col == 1) ? 0x41 : (col == 2) ? 0x22 : (col == 3) ? 0x14 : (col == 4) ? 0x08 : 0;
        case '<': return (col == 3) ? 0x41 : (col == 2) ? 0x22 : (col == 1) ? 0x14 : (col == 0) ? 0x08 : 0;
        case '.': return (col == 2) ? 0x40 : 0;
        case '&': return (col == 0) ? 0x36 : (col == 1) ? 0x49 : (col == 2) ? 0x55 : (col == 3) ? 0x22 : 0x50;
        case '%': return (col == 0 || col == 4) ? 0x63 :
                         (col == 1) ? 0x13 : (col == 2) ? 0x08 : 0x64;
        default: return 0;
    }
}

static uint16_t TextWidth(const char *text, uint8_t scale)
{
    uint16_t len = 0;
    while (text[len] != '\0') len++;
    return len ? (uint16_t)((len * 6 - 1) * scale) : 0;
}

static void Text(int16_t x, int16_t y, const char *text, uint16_t color, uint8_t scale)
{
    uint8_t col, bit, bits;
    if ((y + 7 * scale <= stripe_top) || (y >= stripe_top + BAND_HEIGHT)) return;
    while ((*text != '\0') && (x < UI_WIDTH))
    {
        for (col = 0; col < 5; col++)
        {
            bits = Glyph(*text, col);
            for (bit = 0; bit < 7; bit++)
            {
                if ((bits >> bit) & 1)
                {
                    Rect(x + col * scale, y + bit * scale, scale, scale, color);
                }
            }
        }
        x += 6 * scale;
        text++;
    }
}

static void Center(int16_t y, const char *text, uint16_t color, uint8_t scale)
{
    Text((int16_t)((UI_WIDTH - TextWidth(text, scale)) / 2), y, text, color, scale);
}

static void Number(uint16_t value, int16_t y, uint16_t color)
{
    static const uint8_t segments[10] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    uint8_t digits[3];
    uint8_t i, mask;
    int16_t x;
    int16_t offset = (value < 10) ? 20 : (value < 100) ? 10 : 0;

    digits[0] = (uint8_t)(value / 100 % 10);
    digits[1] = (uint8_t)(value / 10 % 10);
    digits[2] = (uint8_t)(value % 10);
    for (i = 0; i < 3; i++)
    {
        if (((i == 0) && (value < 100)) || ((i == 1) && (value < 10))) continue;
        x = (int16_t)(35 + i * 20 - offset);
        mask = segments[digits[i]];
        if (mask & 0x01) Rect(x + 3, y, 10, 3, color);
        if (mask & 0x02) Rect(x + 13, y + 3, 3, 10, color);
        if (mask & 0x04) Rect(x + 13, y + 16, 3, 10, color);
        if (mask & 0x08) Rect(x + 3, y + 26, 10, 3, color);
        if (mask & 0x10) Rect(x, y + 16, 3, 10, color);
        if (mask & 0x20) Rect(x, y + 3, 3, 10, color);
        if (mask & 0x40) Rect(x + 3, y + 13, 10, 3, color);
    }
}

static uint8_t Progress(const AppSnapshot *snapshot)
{
    uint32_t elapsed;
    if (snapshot->total_ms == 0) return 0;
    elapsed = (snapshot->remaining_ms < snapshot->total_ms)
            ? snapshot->total_ms - snapshot->remaining_ms : 0;
    return (uint8_t)(elapsed * 100 / snapshot->total_ms);
}

static void TimeLabel(uint16_t seconds, int16_t y, uint8_t completed)
{
    char line[] = "SET    S";
    if (completed)
    {
        line[0] = 'R';
        line[1] = 'U';
        line[2] = 'N';
    }
    line[4] = seconds >= 100U ? (char)('0' + seconds / 100U % 10U) : ' ';
    line[5] = seconds >= 10U ? (char)('0' + seconds / 10U % 10U) : ' ';
    line[6] = (char)('0' + seconds % 10U);
    Center(y, line, C_TEXT, 1);
}

static const char *NoticeText(uint8_t notice)
{
    switch (notice)
    {
        case NOTICE_CLOSE_DOOR: return "CLOSE DOOR FIRST";
        case NOTICE_SET_TIME: return "SET TIME FIRST";
        case NOTICE_RUNNING: return "ALREADY RUNNING";
        case NOTICE_PAUSE_FIRST: return "OPEN TO PAUSE";
        case NOTICE_RESUME: return "HOLD K1 TO RESUME";
        case NOTICE_ENTER_TIME: return "SHORT K2 FOR TIME";
        case NOTICE_OVER_TEMP: return "OVER TEMP";
        case NOTICE_TEMP_NO_DATA: return "TEMP NO DATA";
        default: return "";
    }
}

static void Footer(void)
{
    const char *a = "";
    const char *b = "";
    const char *c = "";
    Card(117, 38);
    if (view.notice != NOTICE_NONE)
    {
        Center(126, NoticeText(view.notice), C_AMBER, 1);
        Center(141, "TRY THE NEXT STEP", C_MUTED, 1);
        return;
    }
    switch (view.page)
    {
        case PAGE_WELCOME:
        case PAGE_IDLE:
            a = "K1  CLOSE & ENTER"; b = "WAITING FOR YOU"; break;
        case PAGE_HOME:
            a = "K2  SET TIME"; b = "HOLD K1  START"; c = "K1  OPEN DOOR"; break;
        case PAGE_TIME:
            a = "K2  +20 SECONDS"; b = "HOLD K2  POWER"; c = "HOLD K1  START"; break;
        case PAGE_POWER:
            a = "K2  NEXT LEVEL"; b = "HOLD K2  TIME"; c = "HOLD K1  START"; break;
        case PAGE_RUNNING:
            a = "K1  OPEN TO PAUSE";
#if RELAY_LED_COUNTDOWN_TEST
            b = "HOLD K2  TEST STOP";
#else
            b = "CYCLE IN PROGRESS";
#endif
            break;
        case PAGE_PAUSED:
#if RELAY_LED_COUNTDOWN_TEST
            if (view.door_closed && !view.door_feedback_closed)
            {
                a = "CLOSE DOOR FIRST"; b = "THEN HOLD K1";
                break;
            }
#endif
            a = view.door_closed ? "HOLD K1  RESUME" : "K1  CLOSE DOOR";
            b = view.door_closed ? "K1  REOPEN DOOR" : "THEN HOLD K1";
            break;
        case PAGE_COMPLETED:
            a = "K1  NEXT CYCLE"; b = "READY WHEN YOU ARE"; break;
        case PAGE_STOPPED:
            a = "K1  NEW CYCLE"; b = "SET TIME AGAIN"; break;
        default: break;
    }
    Text(13, 124, a, C_TEXT, 1);
    Text(13, 135, b, C_MUTED, 1);
    Text(13, 146, c, C_MUTED, 1);
}

static void Header(void)
{
    const char *title = "MICROWAVE";
    const char *crumb = "CONTROL SYSTEM";
    char step[3] = {'0','1','\0'};
    uint16_t accent = C_ACCENT;

    switch (view.page)
    {
        case PAGE_IDLE: title = "STANDBY"; crumb = "WAITING FOR START"; break;
        case PAGE_HOME: title = "HOME"; crumb = "READY TO COOK"; step[1] = '2'; break;
        case PAGE_TIME: title = "TIME"; crumb = "SETUP / 1 OF 2"; step[1] = '3'; break;
        case PAGE_POWER: title = "POWER"; crumb = "SETUP / 2 OF 2"; step[1] = '4'; break;
        case PAGE_RUNNING: title = "HEATING"; crumb = "COOKING CYCLE"; step[1] = '5'; accent = C_GREEN; break;
        case PAGE_PAUSED: title = "PAUSED"; crumb = "CYCLE ON HOLD"; step[1] = '5'; accent = C_AMBER; break;
        case PAGE_COMPLETED: title = "COMPLETE"; crumb = "UNTIL NEXT TIME"; step[1] = '6'; accent = C_GREEN; break;
        case PAGE_STOPPED: title = "STOPPED"; crumb = "HEATING STOPPED"; accent = C_AMBER; break;
        default: break;
    }
#if RELAY_LED_COUNTDOWN_TEST
    /* 显示输出层执行的命令，通过控制任务快照传递。 */
    if (view.page == PAGE_RUNNING) { title = "RUNNING"; }
    crumb = view.relay_active ? "LED TEST / PA2 ON" : "LED TEST / PA2 OFF";
#endif
    Rect(0, 0, UI_WIDTH, 2, accent);
    Text(8, 10, title, C_TEXT, 1);
    RoundRect(99, 7, 22, 14, 5, C_CARD);
    Text(104, 11, step, accent, 1);
#if RELAY_LED_COUNTDOWN_TEST
    /* 与 PA2 命令分行，所有页面都能查看实际触点。 */
    Text(8, 20, crumb, C_MUTED, 1);
    Text(8, 29, view.door_feedback_closed ? "PB10 CLOSED" : "PB10 OPEN", C_MUTED, 1);
#else
    Text(8, 25, crumb, C_MUTED, 1);
#endif
    if (animating) Text(113, 25, direction_back ? "<" : ">", accent, 1);
}

static void CheckIcon(void)
{
    int16_t i;
    RoundRect(46, 44, 36, 32, 9, RGB(20, 66, 58));
    for (i = 0; i < 8; i++) Rect(51 + i, 58 + i, 3, 3, C_GREEN);
    for (i = 0; i < 15; i++) Rect(58 + i, 65 - i, 3, 3, C_GREEN);
}

static void Scene(void)
{
    uint8_t i;
    uint8_t shown_door_closed = view.door_closed;
    int16_t x;
    uint16_t selection;
    static const char *const names[3] = {"LOW", "MEDIUM", "HIGH"};
    static const char *const short_names[3] = {"LOW", "MED", "HIGH"};

#if RELAY_PULSE_TEST
    if (view.page == PAGE_RELAY_TEST)
    {
#if PTC_BENCH_TEST
        Center(13, "PTC TEST", C_TEXT, 2);
        Center(34, "5V / 10 SEC", C_AMBER, 1);
#else
        Center(13, "RELAY TEST", C_TEXT, 2);
        Center(34, "NO LOAD", C_AMBER, 1);
#endif
        Card(47, 62);
        Center(58, "PA2 COMMAND", C_MUTED, 1);
        Center(77, view.relay_active ? "ON" : "OFF",
               view.relay_active ? C_AMBER : C_GREEN, 2);
#if PTC_BENCH_TEST
        /* 结束后保持锁定，需复位才能重新武装试热。 */
        Center(119, view.relay_test_finished ? "TEST COMPLETE" : "HOLD K1 TO START",
               C_TEXT, 1);
        Center(134, view.relay_test_finished ? "RESET TO RETEST" : "K2 TO STOP",
               C_MUTED, 1);
#else
        Center(128, "HOLD K1 FOR 1 SEC", C_TEXT, 1);
#endif
        return;
    }
#endif

#if RELAY_LED_COUNTDOWN_TEST
    shown_door_closed = view.door_feedback_closed;
#endif
    Header();
    switch (view.page)
    {
        case PAGE_WELCOME:
        case PAGE_IDLE:
            Card(38, 70);
            RoundRect(36, 47, 56, 35, 6, C_BORDER);
            RoundRect(39, 50, 42, 29, 4, RGB(6, 23, 38));
            Rect(83, 53, 5, 3, C_ACCENT);
            Rect(84, 62, 3, 8, C_BLUE);
            Rect(45, 72, 29, 2, C_ACCENT);
            Rect(45, 57, 2, 10, C_BORDER);
            Rect(50, 54, 2, 13, C_BORDER);
            Center(91, shown_door_closed ? "DOOR CLOSED" : "DOOR OPEN", C_GREEN, 1);
#ifdef FIRMWARE_VERSION
            Center(100, "FW " FIRMWARE_VERSION, C_MUTED, 1);
#endif
            break;
        case PAGE_HOME:
            Card(38, 70);
            Center(46, "READY", C_GREEN, 2);
            Center(77, shown_door_closed ? "DOOR CLOSED" : "DOOR OPEN", C_MUTED, 1);
            TimeLabel(view.cook_seconds, 94, 0);
            Center(65, names[view.power_level % 3], C_TEXT, 1);
            break;
        case PAGE_TIME:
            Card(38, 70);
            Center(46, "COOK TIME", C_MUTED, 1);
            Number(view.cook_seconds, 59, C_TEXT);
            Text(96, 79, "S", C_ACCENT, 1);
            Rect(17, 99, 94, 2, C_BORDER);
            Rect(17, 99, (int16_t)(view.cook_seconds * 94 / 200), 2, C_ACCENT);
            break;
        case PAGE_POWER:
            Card(38, 70);
            Center(46, "POWER LEVEL", C_MUTED, 1);
            Center(61, names[view.power_level % 3], C_TEXT, 2);
            for (i = 0; i < 3; i++)
            {
                x = (int16_t)(14 + i * 34);
                selection = (i == view.power_level) ? C_ACCENT : C_BORDER;
                RoundRect(x, 85, 31, 16, 5, selection);
                Text(x + (31 - TextWidth(short_names[i],1)) / 2, 90,
                     short_names[i], (i == view.power_level) ? C_BG : C_MUTED, 1);
            }
            break;
        case PAGE_RUNNING:
        case PAGE_PAUSED:
            Card(38, 73);
            Center(45, "REMAINING", C_MUTED, 1);
            Number(view.display_seconds, 57, (view.page == PAGE_PAUSED) ? C_AMBER : C_TEXT);
            Text(96, 77, "S", C_ACCENT, 1);
            if (view.page == PAGE_RUNNING)
            {
                Text(15, 91, "POWER", C_MUTED, 1);
                Text(56, 91, names[view.power_level % 3], C_ACCENT, 1);
                Rect(15, 103, 98, 3, C_BORDER);
                Rect(15, 103, (int16_t)(Progress(&view) * 98 / 100), 3, C_GREEN);
            }
            else
            {
                Center(94, shown_door_closed ? "DOOR CLOSED" : "DOOR OPEN", C_AMBER, 1);
            }
            break;
        case PAGE_COMPLETED:
            Card(38, 70);
            CheckIcon();
            Center(87, "ALL DONE", C_GREEN, 1);
            TimeLabel((uint16_t)(view.total_ms / 1000U), 99, 1);
            break;
        case PAGE_STOPPED:
            Card(38, 70);
            Center(59, "HEATING STOPPED", C_AMBER, 1);
            if (view.stop_reason == STOP_OVER_TEMP) { Center(83, "OVER TEMP", C_AMBER, 1); }
            else if (view.stop_reason == STOP_TEMP_NO_DATA) { Center(83, "TEMP NO DATA", C_AMBER, 1); }
            else { Center(83, "TEST STOP", C_MUTED, 1); }
            break;
        default: break;
    }
    /* 环境温度占独立的七像素行，湿度保留在 DHT11 原始读取中。 */
    if (view.environment.status == DHT11_OK)
    {
        uint8_t temperature = view.environment.temperature_c;
        char line[] = "TEMP 00C";
        line[5] = (char)('0' + (temperature / 10U) % 10U);
        line[6] = (char)('0' + temperature % 10U);
        Center(110, line, C_TEXT, 1);
    }
    else
    {
        Center(110, view.environment.status == DHT11_WAITING ? "TEMP WAIT" :
               "TEMP NO DATA", C_AMBER, 1);
    }
    Footer();
}

static void RenderBand(uint8_t band, uint8_t front)
{
    uint16_t i;
    stripe_top = (int16_t)(band * BAND_HEIGHT);
    for (i = 0; i < UI_WIDTH * BAND_HEIGHT; i++) stripe[i] = C_BG;
    Scene();
    if (front) Rect(0, stripe_top + BAND_HEIGHT - 1, UI_WIDTH, 1, C_ACCENT);
    LCD_Blit(0, (uint16_t)stripe_top, UI_WIDTH, BAND_HEIGHT, stripe);
    ui_rendered_bands++;
}

static void DirtyRegion(uint8_t y0, uint8_t y1)
{
    uint8_t band;
    for (band = y0 / BAND_HEIGHT; band <= y1 / BAND_HEIGHT; band++)
    {
        dirty_bands |= 1UL << band;
    }
}

void UI_Init(void)
{
    has_snapshot = 0;
    animating = 0;
    animation_band = 0;
    previous_front = -1;
    direction_back = 0;
    dirty_bands = 0;
    last_render = 0;
    ui_rendered_bands = 0;
    ui_transition_count = 0;
}

void UI_SetSnapshot(const AppSnapshot *snapshot, uint32_t now)
{
    uint8_t page_changed = (uint8_t)(!has_snapshot || (view.page != snapshot->page));
    if (page_changed)
    {
        direction_back = (uint8_t)(has_snapshot && (snapshot->page < view.page));
        /* Pause and completion preempt a decorative transition immediately. */
        animating = (uint8_t)(has_snapshot && (snapshot->page != PAGE_PAUSED) &&
                             (snapshot->page != PAGE_COMPLETED) && (snapshot->page != PAGE_STOPPED));
        animation_band = 0;
        previous_front = -1;
        dirty_bands = ALL_BANDS;
        last_render = now - 10;
        ui_transition_count++;
    }
    else
    {
        if ((view.display_seconds != snapshot->display_seconds) ||
            (view.cook_seconds != snapshot->cook_seconds)) DirtyRegion(56, 101);
        if (view.power_level != snapshot->power_level) DirtyRegion(38, 110);
        if (view.door_closed != snapshot->door_closed)
        {
            DirtyRegion(88, 106);
            DirtyRegion(117, 154);
        }
#if RELAY_LED_COUNTDOWN_TEST
        if (view.relay_active != snapshot->relay_active) DirtyRegion(20, 26);
        if (view.door_feedback_closed != snapshot->door_feedback_closed)
        {
            DirtyRegion(20, 35);
            DirtyRegion(77, 100);
            DirtyRegion(117, 154);
        }
#endif
        if (view.notice != snapshot->notice) DirtyRegion(117, 154);
        if (Progress(&view) != Progress(snapshot)) DirtyRegion(103, 106);
#if RELAY_PULSE_TEST
        /* 单页诊断只重绘 PA2 指令所在条带。 */
        if (view.relay_active != snapshot->relay_active) DirtyRegion(77, 90);
#endif
#if PTC_BENCH_TEST
        if (view.relay_test_finished != snapshot->relay_test_finished) DirtyRegion(119, 140);
#endif
        if (view.environment.status != snapshot->environment.status ||
            view.environment.temperature_c != snapshot->environment.temperature_c)
        {
            DirtyRegion(110, 116);
        }
    }
    view = *snapshot;
    has_snapshot = 1;
}

void UI_RenderStep(uint32_t now)
{
    uint8_t band;
    uint8_t count;
    if (!has_snapshot || ((uint32_t)(now - last_render) < 10)) return;
    last_render = now;
    if (animating)
    {
        if (previous_front >= 0)
        {
            RenderBand((uint8_t)previous_front, 0);
            dirty_bands &= ~(1UL << previous_front);
        }
        band = animation_band++;
        RenderBand(band, (uint8_t)(animation_band < BAND_COUNT));
        dirty_bands &= ~(1UL << band);
        previous_front = (int8_t)band;
        if (animation_band >= BAND_COUNT)
        {
            animating = 0;
            previous_front = -1;
            /* The direction chevron belongs only to the transition. */
            DirtyRegion(24, 31);
        }
        return;
    }
    for (count = 0; count < 2; count++)
    {
        if (dirty_bands == 0) return;
        for (band = 0; band < BAND_COUNT; band++)
        {
            if (dirty_bands & (1UL << band)) break;
        }
        RenderBand(band, 0);
        dirty_bands &= ~(1UL << band);
    }
}
