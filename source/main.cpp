#include <switch.h>
#include <cstdio>
#include <cstring>

// Phone UI is centered in the 80x44 console grid.
// Phone frame: x = 20..60 (width 40), y = 0..43 (height 44)
static const int PHONE_X = 20;
static const int PHONE_W = 40;
static const int PHONE_Y = 0;
static const int PHONE_H = 44;

static char g_screen[44][81];

enum AppState {
    STATE_LOCK,
    STATE_START,
    STATE_PHONE,
    STATE_MESSAGING,
    STATE_SETTINGS,
    STATE_ABOUT
};
static AppState g_state = STATE_LOCK;

static int g_tile_cursor = 0;
static const int TILE_COUNT = 6;

static const char* g_tile_names[TILE_COUNT] = {
    "Phone",
    "Messaging",
    "Email",
    "Store",
    "Photos",
    "Settings"
};

static const char* g_tile_icons[TILE_COUNT] = {
    " [PHONE] ",
    " [MSG]   ",
    " [MAIL]  ",
    " [STORE] ",
    " [PICS]  ",
    " [GEAR]  "
};

static PadState g_pad;

static void clearBuf() {
    for (int y = 0; y < 44; y++) {
        for (int x = 0; x < 80; x++) g_screen[y][x] = ' ';
        g_screen[y][80] = 0;
    }
}

static void putStr(int x, int y, const char* s) {
    if (y < 0 || y >= 44) return;
    for (int i = 0; s[i] && x + i < 80; i++) {
        if (x + i >= 0) g_screen[y][x + i] = s[i];
    }
}

static void fillRect(int x, int y, int w, int h, char c) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int px = x + i, py = y + j;
            if (px >= 0 && px < 80 && py >= 0 && py < 44) g_screen[py][px] = c;
        }
}

static void drawPhoneFrame() {
    // Draw the phone bezel
    for (int y = PHONE_Y; y < PHONE_Y + PHONE_H; y++) {
        g_screen[y][PHONE_X] = '|';
        g_screen[y][PHONE_X + PHONE_W - 1] = '|';
    }
    for (int x = PHONE_X; x < PHONE_X + PHONE_W; x++) {
        g_screen[PHONE_Y][x] = '-';
        g_screen[PHONE_Y + PHONE_H - 1][x] = '-';
    }
    g_screen[PHONE_Y][PHONE_X] = '+';
    g_screen[PHONE_Y][PHONE_X + PHONE_W - 1] = '+';
    g_screen[PHONE_Y + PHONE_H - 1][PHONE_X] = '+';
    g_screen[PHONE_Y + PHONE_H - 1][PHONE_X + PHONE_W - 1] = '+';
}

static void drawStatusBar() {
    // Top status bar inside phone
    int y = PHONE_Y + 1;
    for (int x = PHONE_X + 1; x < PHONE_X + PHONE_W - 1; x++) g_screen[y][x] = ' ';
    putStr(PHONE_X + 2, y, "LTE");
    putStr(PHONE_X + PHONE_W - 8, y, "10:40");
}

static void drawLockScreen() {
    int cx = PHONE_X + PHONE_W / 2;
    putStr(cx - 5, 10, "22:34");
    putStr(cx - 12, 12, "Wednesday, October 7");
    putStr(cx - 12, 30, "Press A to unlock");
}

static void drawStartScreen() {
    drawStatusBar();
    int startY = 4;
    int tileW = 12;
    int tileH = 5;

    for (int i = 0; i < TILE_COUNT; i++) {
        int col = i % 2;
        int row = i / 2;
        int tx = PHONE_X + 2 + col * (tileW + 2);
        int ty = startY + row * (tileH + 1);

        bool selected = (i == g_tile_cursor);
        char border = selected ? '#' : '+';
        char fill   = selected ? '=' : ' ';

        // Top border
        for (int x = 0; x < tileW; x++) g_screen[ty][tx + x] = (x == 0 || x == tileW - 1) ? border : '-';
        // Bottom border
        for (int x = 0; x < tileW; x++) g_screen[ty + tileH - 1][tx + x] = (x == 0 || x == tileW - 1) ? border : '-';
        // Sides and fill
        for (int y = 1; y < tileH - 1; y++) {
            g_screen[ty + y][tx] = border;
            g_screen[ty + y][tx + tileW - 1] = border;
            for (int x = 1; x < tileW - 1; x++) g_screen[ty + y][tx + x] = fill;
        }

        putStr(tx + 2, ty + 2, g_tile_icons[i]);
        putStr(tx + 2, ty + 4, g_tile_names[i]);
    }
}

static void drawPhoneApp() {
    drawStatusBar();
    int cx = PHONE_X + PHONE_W / 2;
    putStr(cx - 5, 6, "[ 1 ] [ 2 ] [ 3 ]");
    putStr(cx - 5, 8, "[ 4 ] [ 5 ] [ 6 ]");
    putStr(cx - 5, 10, "[ 7 ] [ 8 ] [ 9 ]");
    putStr(cx - 5, 12, "[ * ] [ 0 ] [ # ]");
    putStr(cx - 10, 20, "Calling...");
    putStr(cx - 12, 22, "Press B to hang up");
}

static void drawMessagingApp() {
    drawStatusBar();
    putStr(PHONE_X + 2, 4, "> Alex");
    putStr(PHONE_X + 4, 5, "Hey, what's up?");
    putStr(PHONE_X + 2, 7, "> Mom");
    putStr(PHONE_X + 4, 8, "Don't forget milk!");
    putStr(PHONE_X + 2, 10, "> Work");
    putStr(PHONE_X + 4, 11, "Meeting at 3pm.");
    putStr(PHONE_X + 2, 30, "Press B to go back");
}

static void drawSettingsApp() {
    drawStatusBar();
    putStr(PHONE_X + 2, 4, "Settings");
    putStr(PHONE_X + 2, 6, "[X] Wi-Fi");
    putStr(PHONE_X + 2, 7, "[ ] Bluetooth");
    putStr(PHONE_X + 2, 8, "[ ] Airplane Mode");
    putStr(PHONE_X + 2, 10, "[X] Location");
    putStr(PHONE_X + 2, 11, "[ ] Battery Saver");
    putStr(PHONE_X + 2, 30, "Press B to go back");
}

static void drawAboutApp() {
    drawStatusBar();
    putStr(PHONE_X + 2, 4, "About");
    putStr(PHONE_X + 2, 6, "Windows 10 Mobile");
    putStr(PHONE_X + 2, 7, "Simulator for Switch");
    putStr(PHONE_X + 2, 9, "Version 1.0");
    putStr(PHONE_X + 2, 30, "Press B to go back");
}

static void render() {
    clearBuf();
    drawPhoneFrame();

    switch (g_state) {
        case STATE_LOCK:       drawLockScreen(); break;
        case STATE_START:      drawStartScreen(); break;
        case STATE_PHONE:      drawPhoneApp(); break;
        case STATE_MESSAGING:  drawMessagingApp(); break;
        case STATE_SETTINGS:   drawSettingsApp(); break;
        case STATE_ABOUT:      drawAboutApp(); break;
    }

    // Bottom navigation bar
    int by = PHONE_Y + PHONE_H - 3;
    for (int x = PHONE_X + 1; x < PHONE_X + PHONE_W - 1; x++) g_screen[by][x] = '-';
    putStr(PHONE_X + 4, by + 1, "<");
    putStr(PHONE_X + PHONE_W / 2 - 1, by + 1, "[]");
    putStr(PHONE_X + PHONE_W - 6, by + 1, ">");
}

static void flush() {
    consoleClear();
    for (int y = 0; y < 44; y++) {
        printf("%s", g_screen[y]);
        if (y < 43) printf("\n");
    }
    consoleUpdate(NULL);
}

int main(int, char**) {
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&g_pad);

    while (appletMainLoop()) {
        padUpdate(&g_pad);
        u64 kDown = padGetButtonsDown(&g_pad);
        if (kDown & HidNpadButton_Plus) break;

        switch (g_state) {
            case STATE_LOCK:
                if (kDown & HidNpadButton_A) g_state = STATE_START;
                break;

            case STATE_START:
                if (kDown & HidNpadButton_Left)  g_tile_cursor = (g_tile_cursor % 2 == 1) ? g_tile_cursor - 1 : g_tile_cursor;
                if (kDown & HidNpadButton_Right) g_tile_cursor = (g_tile_cursor % 2 == 0 && g_tile_cursor + 1 < TILE_COUNT) ? g_tile_cursor + 1 : g_tile_cursor;
                if (kDown & HidNpadButton_Up)    g_tile_cursor = (g_tile_cursor >= 2) ? g_tile_cursor - 2 : g_tile_cursor;
                if (kDown & HidNpadButton_Down)  g_tile_cursor = (g_tile_cursor + 2 < TILE_COUNT) ? g_tile_cursor + 2 : g_tile_cursor;

                if (kDown & HidNpadButton_A) {
                    switch (g_tile_cursor) {
                        case 0: g_state = STATE_PHONE; break;
                        case 1: g_state = STATE_MESSAGING; break;
                        case 2: g_state = STATE_ABOUT; break; // Email placeholder
                        case 3: g_state = STATE_ABOUT; break; // Store placeholder
                        case 4: g_state = STATE_ABOUT; break; // Photos placeholder
                        case 5: g_state = STATE_SETTINGS; break;
                    }
                }
                if (kDown & HidNpadButton_B) g_state = STATE_LOCK;
                break;

            case STATE_PHONE:
            case STATE_MESSAGING:
            case STATE_SETTINGS:
            case STATE_ABOUT:
                if (kDown & HidNpadButton_B) g_state = STATE_START;
                break;
        }

        render();
        flush();
    }

    consoleExit(NULL);
    return 0;
}
