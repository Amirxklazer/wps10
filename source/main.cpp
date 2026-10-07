#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstdio>
#include <cstring>

static SDL_Window*   g_win = nullptr;
static SDL_Renderer* g_ren = nullptr;
static TTF_Font*     g_font_xl = nullptr;
static TTF_Font*     g_font_lg = nullptr;
static TTF_Font*     g_font_md = nullptr;
static TTF_Font*     g_font_sm = nullptr;

static const int SW = 1280;
static const int SH = 720;
static const int PHONE_W = 440;
static const int PHONE_H = 700;
static const int PHONE_X = (SW - PHONE_W) / 2;
static const int PHONE_Y = (SH - PHONE_H) / 2;

enum Screen { SC_LOCK, SC_START, SC_PHONE, SC_MSG, SC_SETTINGS, SC_ABOUT };
static Screen g_screen = SC_LOCK;
static int    g_tile = 0;

struct Tile {
    const char* label;
    Uint8 r, g, b;
    int x, y, w, h;
    Screen target;
    bool wide;
};

static Tile g_tiles[] = {
    { "Phone",    0, 120, 215,   0,   0, 200, 200, SC_PHONE,    false },
    { "Messaging",0,  90, 158, 204,   0, 200,  98, SC_MSG,      false },
    { "Email",    0,  90, 158, 204, 102, 200,  98, SC_ABOUT,    false },
    { "Store",    0, 120, 215,   0, 204, 200,  98, SC_ABOUT,    false },
    { "Photos",  27, 161, 226, 204, 204, 200, 200, SC_ABOUT,    false },
    { "Settings", 60,  60,  60,   0, 306, 200,  98, SC_SETTINGS, false },
};
static const int TILE_COUNT = 6;

static PadState g_pad;

// ---------- helpers ----------
static void setColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) {
    SDL_SetRenderDrawColor(g_ren, r, g, b, a);
}

static void fillRect(int x, int y, int w, int h) {
    SDL_Rect r = { x, y, w, h };
    SDL_RenderFillRect(g_ren, &r);
}

static void drawText(int x, int y, const char* s, TTF_Font* f, SDL_Color c) {
    if (!s || !*s || !f) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, s, c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(g_ren, surf);
    if (!tex) { SDL_FreeSurface(surf); return; }
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(g_ren, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void drawTextCenter(int cx, int y, const char* s, TTF_Font* f, SDL_Color c) {
    if (!s || !*s || !f) return;
    int w = 0, h = 0;
    if (TTF_SizeUTF8(f, s, &w, &h) != 0) return;
    drawText(cx - w / 2, y, s, f, c);
}

static void drawTextRight(int rx, int y, const char* s, TTF_Font* f, SDL_Color c) {
    if (!s || !*s || !f) return;
    int w = 0, h = 0;
    if (TTF_SizeUTF8(f, s, &w, &h) != 0) return;
    drawText(rx - w, y, s, f, c);
}

// ---------- font loading from Switch shared font ----------
static TTF_Font* loadSharedFont(int size) {
    PlFontData fd;
    if (R_FAILED(plGetSharedFontByType(&fd, PlSharedFontType_Standard))) return nullptr;
    SDL_RWops* rw = SDL_RWFromConstMem(fd.address, fd.size);
    if (!rw) return nullptr;
    return TTF_OpenFontRW(rw, 1, size);
}

static bool loadFonts() {
    g_font_xl = loadSharedFont(96);
    g_font_lg = loadSharedFont(48);
    g_font_md = loadSharedFont(26);
    g_font_sm = loadSharedFont(20);
    return g_font_xl && g_font_lg && g_font_md && g_font_sm;
}

// ---------- screens ----------
static void drawStatusBar() {
    setColor(0, 0, 0);
    fillRect(PHONE_X, PHONE_Y, PHONE_W, 36);
    SDL_Color w = {255,255,255,255};
    drawText(PHONE_X + 14, PHONE_Y + 6, "LTE", g_font_sm, w);
    drawText(PHONE_X + 60, PHONE_Y + 6, "|||", g_font_sm, w);
    drawTextRight(PHONE_X + PHONE_W - 14, PHONE_Y + 6, "10:40", g_font_sm, w);
}

static void drawLockScreen() {
    // sky
    setColor(30, 90, 180);
    fillRect(PHONE_X, PHONE_Y, PHONE_W, PHONE_H / 2);
    // sunset
    setColor(230, 140, 60);
    fillRect(PHONE_X, PHONE_Y + PHONE_H / 2, PHONE_W, PHONE_H / 2);
    // cliff silhouette
    setColor(90, 40, 20);
    SDL_Rect c1 = { PHONE_X + 60, PHONE_Y + PHONE_H/2 + 40, 120, 160 };
    SDL_Rect c2 = { PHONE_X + 220, PHONE_Y + PHONE_H/2 + 10, 160, 190 };
    SDL_RenderFillRect(g_ren, &c1);
    SDL_RenderFillRect(g_ren, &c2);
    // clock
    SDL_Color w = {255,255,255,255};
    drawTextCenter(PHONE_X + PHONE_W/2, PHONE_Y + 200, "22:34", g_font_xl, w);
    drawTextCenter(PHONE_X + PHONE_W/2, PHONE_Y + 310, "Wednesday, 7 October", g_font_md, w);
    drawTextCenter(PHONE_X + PHONE_W/2, PHONE_Y + PHONE_H - 80, "Press A to unlock", g_font_sm, w);
}

static void drawTile(const Tile& t, bool selected) {
    int px = PHONE_X + 12 + t.x;
    int py = PHONE_Y + 48 + t.y;

    if (selected) {
        setColor(255, 255, 255);
        fillRect(px - 3, py - 3, t.w + 6, t.h + 6);
    }
    setColor(t.r, t.g, t.b);
    fillRect(px, py, t.w, t.h);

    SDL_Color w = {255,255,255,255};
    int ty = py + t.h - 34;
    drawText(px + 14, ty, t.label, g_font_sm, w);
}

static void drawStartScreen() {
    setColor(20, 20, 20);
    fillRect(PHONE_X, PHONE_Y, PHONE_W, PHONE_H);
    drawStatusBar();
    for (int i = 0; i < TILE_COUNT; i++)
        drawTile(g_tiles[i], i == g_tile);
}

static void drawBottomNav(const char* left, const char* mid, const char* right) {
    int ny = PHONE_Y + PHONE_H - 54;
    setColor(0, 0, 0);
    fillRect(PHONE_X, ny, PHONE_W, 54);
    SDL_Color w = {255,255,255,255};
    drawText(PHONE_X + 30, ny + 14, left, g_font_md, w);
    drawTextCenter(PHONE_X + PHONE_W/2, ny + 14, mid, g_font_md, w);
    drawTextRight(PHONE_X + PHONE_W - 30, ny + 14, right, g_font_md, w);
}

static void drawPhoneApp() {
    setColor(20, 20, 20);
    fillRect(PHONE_X, PHONE_Y, PHONE_W, PHONE_H);
    drawStatusBar();
    SDL_Color w = {255,255,255,255};

    const char* keys[3][3] = {
        {"1","2","3"}, {"4","5","6"}, {"7","8","9"}
    };
    int bx = PHONE_X + 40;
    int by = PHONE_Y + 90;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            int x = bx + c * 120;
            int y = by + r * 100;
            setColor(40, 40, 40);
            fillRect(x, y, 110, 90);
            drawTextCenter(x + 55, y + 25, keys[r][c], g_font_lg, w);
        }
    }
    drawTextCenter(PHONE_X + PHONE_W/2, PHONE_Y + 420, "Calling...", g_font_md, w);
    drawBottomNav("<", "O", ">");
}

static void drawMsgApp() {
    setColor(20, 20, 20);
    fillRect(PHONE_X, PHONE_Y, PHONE_W, PHONE_H);
    drawStatusBar();
    SDL_Color w = {255,255,255,255};

    const char* names[] = { "Alex", "Mom", "Work", "Sam" };
    const char* prev[]  = { "Hey, what's up?", "Don't forget milk!",
                            "Meeting at 3pm.", "See you soon" };
    for (int i = 0; i < 4; i++) {
        int y = PHONE_Y + 60 + i * 80;
        setColor(40, 40, 40);
        fillRect(PHONE_X + 10, y, PHONE_W - 20, 68);
        drawText(PHONE_X + 26, y + 8, names[i], g_font_md, w);
        setColor(160, 160, 160);
        SDL_Color g = {180,180,180,255};
        drawText(PHONE_X + 26, y + 38, prev[i], g_font_sm, g);
    }
    drawBottomNav("<", "O", ">");
}

static void drawSettingsApp() {
    setColor(20, 20, 20);
    fillRect(PHONE_X, PHONE_Y, PHONE_W, PHONE_H);
    drawStatusBar();
    SDL_Color w = {255,255,255,255};

    const char* items[] = { "Wi-Fi", "Bluetooth", "Airplane Mode",
                            "Location", "Battery Saver", "Brightness" };
    bool on[] = { true, false, false, true, false, true };
    for (int i = 0; i < 6; i++) {
        int y = PHONE_Y + 60 + i * 68;
        setColor(40, 40, 40);
        fillRect(PHONE_X + 10, y, PHONE_W - 20, 56);
        drawText(PHONE_X + 26, y + 12, items[i], g_font_md, w);

        int tx = PHONE_X + PHONE_W - 80;
        if (on[i]) setColor(0, 120, 215);
        else       setColor(90, 90, 90);
        fillRect(tx, y + 12, 60, 32);
        SDL_Color t = {255,255,255,255};
        drawTextCenter(tx + 30, y + 16, on[i] ? "ON" : "OFF", g_font_sm, t);
    }
    drawBottomNav("<", "O", ">");
}

static void drawAboutApp() {
    setColor(20, 20, 20);
    fillRect(PHONE_X, PHONE_Y, PHONE_W, PHONE_H);
    drawStatusBar();
    SDL_Color w = {255,255,255,255};
    drawTextCenter(PHONE_X + PHONE_W/2, PHONE_Y + 120, "Windows 10 Mobile", g_font_lg, w);
    drawTextCenter(PHONE_X + PHONE_W/2, PHONE_Y + 190, "Simulator", g_font_md, w);
    drawTextCenter(PHONE_X + PHONE_W/2, PHONE_Y + 240, "Version 1.0", g_font_sm, w);
    drawTextCenter(PHONE_X + PHONE_W/2, PHONE_Y + 280, "Running on Nintendo Switch", g_font_sm, w);
    drawBottomNav("<", "O", ">");
}

static void render() {
    setColor(0, 0, 0);
    SDL_RenderClear(g_ren);

    // phone bezel
    setColor(10, 10, 10);
    fillRect(PHONE_X - 8, PHONE_Y - 8, PHONE_W + 16, PHONE_H + 16);

    switch (g_screen) {
        case SC_LOCK:     drawLockScreen(); break;
        case SC_START:    drawStartScreen(); break;
        case SC_PHONE:    drawPhoneApp(); break;
        case SC_MSG:      drawMsgApp(); break;
        case SC_SETTINGS: drawSettingsApp(); break;
        case SC_ABOUT:    drawAboutApp(); break;
    }

    SDL_RenderPresent(g_ren);
}

// ---------- main ----------
int main(int, char**) {
    plInitialize(PlServiceType_User);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) { plExit(); return 1; }
    if (TTF_Init() != 0) { SDL_Quit(); plExit(); return 1; }

    SDL_DisplayMode dm;
    SDL_GetCurrentDisplayMode(0, &dm);

    g_win = SDL_CreateWindow("WinPhone",
        0, 0, dm.w, dm.h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN);
    g_ren = SDL_CreateRenderer(g_win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!loadFonts()) {
        // still continue; text just won't draw
    }

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&g_pad);

    bool running = true;
    while (appletMainLoop() && running) {
        padUpdate(&g_pad);
        u64 kDown = padGetButtonsDown(&g_pad);
        if (kDown & HidNpadButton_Plus) break;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
        }

        switch (g_screen) {
            case SC_LOCK:
                if (kDown & HidNpadButton_A) g_screen = SC_START;
                break;

            case SC_START: {
                int col = g_tile % 2;
                int row = g_tile / 2;
                if ((kDown & HidNpadButton_Left)  && col > 0) g_tile--;
                if ((kDown & HidNpadButton_Right) && col < 1 && g_tile + 1 < TILE_COUNT) g_tile++;
                if ((kDown & HidNpadButton_Up)    && row > 0) g_tile -= 2;
                if ((kDown & HidNpadButton_Down)  && g_tile + 2 < TILE_COUNT) g_tile += 2;

                if (kDown & HidNpadButton_A) {
                    g_screen = g_tiles[g_tile].target;
                }
                if (kDown & HidNpadButton_B) g_screen = SC_LOCK;
                break;
            }

            case SC_PHONE:
            case SC_MSG:
            case SC_SETTINGS:
            case SC_ABOUT:
                if (kDown & HidNpadButton_B) g_screen = SC_START;
                break;
        }

        render();
    }

    if (g_font_xl) TTF_CloseFont(g_font_xl);
    if (g_font_lg) TTF_CloseFont(g_font_lg);
    if (g_font_md) TTF_CloseFont(g_font_md);
    if (g_font_sm) TTF_CloseFont(g_font_sm);
    TTF_Quit();
    SDL_DestroyRenderer(g_ren);
    SDL_DestroyWindow(g_win);
    SDL_Quit();
    plExit();
    return 0;
}
