/**
 * @file controls.c
 * @brief Remappable controls + "PS Vita controls" menu (Start+Select), ported from Zenonia 2.
 *
 * Each action (gas, brake, item, pause) is a virtual touch the game receives through OnEvent,
 * like a finger on the iPhone; any bound button held = finger down. Bindings and options persist
 * in ux0:data/crashnitro1/config.txt, hand-editable ("gas=CROSS,R"). The menu pauses the game
 * and is drawn with debugScreen's 8x8 font into a 480x272 texture.
 */
#include <ctype.h>
#include <psp2/ctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "controls.h"
#include "gles.h"
#include "hle.h"
#include "log.h"

#define CONFIG_PATH DATA_PATH "config.txt"

extern const unsigned char font8x8[];

button_action actions[ACT_COUNT] = {
    {"gas", "Accelerate", 440, 270, SCE_CTRL_CROSS},
    {"brake", "Brake / Drift", 40, 270, SCE_CTRL_SQUARE | SCE_CTRL_CIRCLE},
    {"item", "Use item", 440, 50, SCE_CTRL_TRIANGLE},
    {"pause", "Pause", 40, 40, SCE_CTRL_START},
};

int steer_invert = 0, steer_deadzone = 15;
static int stretch = 1;
static const int deadzones[] = {5, 10, 15, 20, 25, 30, 40};
#define NDEADZONES (int)(sizeof(deadzones) / sizeof(deadzones[0]))

static const struct {
    const char *name;
    uint32_t bit;
} btn_names[] = {
    {"CROSS", SCE_CTRL_CROSS},   {"CIRCLE", SCE_CTRL_CIRCLE}, {"SQUARE", SCE_CTRL_SQUARE},
    {"TRIANGLE", SCE_CTRL_TRIANGLE}, {"L", SCE_CTRL_LTRIGGER}, {"R", SCE_CTRL_RTRIGGER},
    {"UP", SCE_CTRL_UP},         {"DOWN", SCE_CTRL_DOWN},     {"LEFT", SCE_CTRL_LEFT},
    {"RIGHT", SCE_CTRL_RIGHT},   {"SELECT", SCE_CTRL_SELECT}, {"START", SCE_CTRL_START},
};
#define BTN_NAME_COUNT (int)(sizeof(btn_names) / sizeof(btn_names[0]))
// START cancela la captura en el menú, así que solo se asigna editando config.txt.
#define BINDABLE_MASK                                                                                       \
    (SCE_CTRL_CROSS | SCE_CTRL_CIRCLE | SCE_CTRL_SQUARE | SCE_CTRL_TRIANGLE | SCE_CTRL_LTRIGGER |             \
     SCE_CTRL_RTRIGGER | SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT | SCE_CTRL_SELECT)

static void set_defaults(void) {
    for (int a = 0; a < ACT_COUNT; a++)
        actions[a].mask = actions[a].def;
    steer_invert = 0;
    steer_deadzone = 15;
    stretch = 1;
    gles_set_stretch(stretch);
}

static void binding_str(uint32_t m, char *out, size_t n, const char *sep) {
    out[0] = 0;
    for (int b = 0; b < BTN_NAME_COUNT; b++)
        if (m & btn_names[b].bit)
            snprintf(out + strlen(out), n - strlen(out), "%s%s", out[0] ? sep : "", btn_names[b].name);
    if (!out[0])
        snprintf(out, n, "NONE");
}

static void controls_save(void) {
    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f) {
        l_error("config: cannot write %s", CONFIG_PATH);
        return;
    }
    char b[96];
    fprintf(f, "# Crash Nitro Kart - PS Vita controls (also editable in game with START+SELECT)\n"
               "# Buttons: CROSS CIRCLE SQUARE TRIANGLE L R UP DOWN LEFT RIGHT SELECT START NONE\n");
    for (int a = 0; a < ACT_COUNT; a++) {
        binding_str(actions[a].mask, b, sizeof(b), ",");
        fprintf(f, "%s=%s\n", actions[a].key, b);
    }
    fprintf(f, "steer_invert=%d\nsteer_deadzone=%d\nstretch=%d\n", steer_invert, steer_deadzone, stretch);
    fclose(f);
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s))
        s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = 0;
    return s;
}

void controls_init(void) {
    set_defaults();
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) {
        controls_save(); // defaults, so the file can be hand-edited
        l_info("config: wrote defaults to %s", CONFIG_PATH);
        return;
    }
    char line[160];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (line[0] == '#' || !eq)
            continue;
        *eq = 0;
        char *key = trim(line), *v = trim(eq + 1);
        if (!strcasecmp(key, "steer_invert"))
            steer_invert = atoi(v) != 0;
        else if (!strcasecmp(key, "steer_deadzone"))
            steer_deadzone = atoi(v);
        else if (!strcasecmp(key, "stretch"))
            stretch = atoi(v) != 0;
        for (int a = 0; a < ACT_COUNT; a++) {
            if (strcasecmp(key, actions[a].key))
                continue;
            actions[a].mask = 0;
            for (char *tok = strtok(v, ","); tok; tok = strtok(NULL, ",")) {
                tok = trim(tok);
                int b = 0;
                while (b < BTN_NAME_COUNT && strcasecmp(tok, btn_names[b].name))
                    b++;
                if (b < BTN_NAME_COUNT)
                    actions[a].mask |= btn_names[b].bit;
                else if (strcasecmp(tok, "NONE"))
                    l_warn("config: unknown button '%s'", tok);
            }
        }
    }
    fclose(f);
    gles_set_stretch(stretch);
    char b[96];
    for (int a = 0; a < ACT_COUNT; a++) {
        binding_str(actions[a].mask, b, sizeof(b), ",");
        l_info("config: %s=%s", actions[a].key, b);
    }
    l_info("config: steer_invert=%d steer_deadzone=%d stretch=%d", steer_invert, steer_deadzone, stretch);
}

/* ------------------------------ menu ------------------------------ */

enum { ROW_STEER = ACT_COUNT, ROW_DEADZONE, ROW_SCREEN, ROW_DEFAULTS, ROW_COUNT };
enum { CAP_NONE, CAP_REPLACE, CAP_ADD };

static int menu_open, cursor, capture = CAP_NONE, dirty = 1;
static uint32_t prev_buttons;

int controls_menu_active(void) { return menu_open; }

int controls_menu_update(uint32_t buttons) {
    uint32_t pressed = buttons & ~prev_buttons;
    prev_buttons = buttons;

    if (!menu_open) {
        if ((buttons & (SCE_CTRL_START | SCE_CTRL_SELECT)) == (SCE_CTRL_START | SCE_CTRL_SELECT) &&
            (pressed & (SCE_CTRL_START | SCE_CTRL_SELECT))) {
            menu_open = 1;
            capture = CAP_NONE;
            dirty = 1;
        }
        return menu_open;
    }
    if (!pressed)
        return 1;
    dirty = 1;

    if (capture != CAP_NONE) {
        if (pressed & SCE_CTRL_START) { // cancel
            capture = CAP_NONE;
            return 1;
        }
        uint32_t bit = pressed & BINDABLE_MASK;
        if (!bit)
            return 1;
        bit &= -bit; // one button per press
        for (int a = 0; a < ACT_COUNT; a++)
            actions[a].mask &= ~bit; // a button belongs to one action only
        actions[cursor].mask = (capture == CAP_ADD ? actions[cursor].mask : 0) | bit;
        capture = CAP_NONE;
        return 1;
    }

    if (pressed & SCE_CTRL_UP)
        cursor = (cursor + ROW_COUNT - 1) % ROW_COUNT;
    if (pressed & SCE_CTRL_DOWN)
        cursor = (cursor + 1) % ROW_COUNT;
    if (pressed & SCE_CTRL_CIRCLE) {
        controls_save();
        menu_open = 0;
        return 0;
    }
    int step = (pressed & SCE_CTRL_LEFT) ? -1 : (pressed & (SCE_CTRL_RIGHT | SCE_CTRL_CROSS)) ? 1 : 0;
    if (cursor < ACT_COUNT) {
        if (pressed & SCE_CTRL_CROSS)
            capture = CAP_REPLACE;
        if (pressed & SCE_CTRL_SQUARE)
            capture = CAP_ADD;
        if (pressed & SCE_CTRL_TRIANGLE)
            actions[cursor].mask = 0;
    } else if (cursor == ROW_STEER && step) {
        steer_invert = !steer_invert;
    } else if (cursor == ROW_DEADZONE && step) {
        int i = 0;
        while (i < NDEADZONES - 1 && deadzones[i] < steer_deadzone)
            i++;
        steer_deadzone = deadzones[(i + step + NDEADZONES) % NDEADZONES];
    } else if (cursor == ROW_SCREEN && step) {
        stretch = !stretch;
        gles_set_stretch(stretch);
    } else if (cursor == ROW_DEFAULTS && (pressed & SCE_CTRL_CROSS)) {
        set_defaults();
    }
    return 1;
}

#define MENU_W 480
#define MENU_H 272
static uint32_t menu_px[MENU_W * MENU_H];
static GLuint menu_tex;

// Colors are ABGR in memory (GL_RGBA/GL_UNSIGNED_BYTE, little endian).
#define C_BG 0xFF201008
#define C_SEL 0xFF603818
#define C_TEXT 0xFFE0E0E0
#define C_DIM 0xFF909090
#define C_HEAD 0xFF40C0FF

static void fill_row(int y, int h, uint32_t c) {
    for (int i = y * MENU_W; i < (y + h) * MENU_W; i++)
        menu_px[i] = c;
}

static void text(int x, int y, uint32_t c, const char *s) {
    for (; *s && x + 8 <= MENU_W; s++, x += 8)
        for (int r = 0; r < 8; r++) {
            unsigned char bits = font8x8[(unsigned char)*s * 8 + r];
            for (int col = 0; col < 8; col++)
                if (bits & (0x80 >> col))
                    menu_px[(y + r) * MENU_W + x + col] = c;
        }
}

static void render(void) {
    fill_row(0, MENU_H, C_BG);
    text(16, 12, C_HEAD, "PS VITA CONTROLS");
    for (int r = 0; r < ROW_COUNT; r++) {
        int y = 36 + r * 16 + (r >= ACT_COUNT ? 8 : 0);
        if (r == cursor)
            fill_row(y - 4, 16, C_SEL);
        char val[96];
        if (r < ACT_COUNT) {
            text(16, y, C_TEXT, actions[r].label);
            if (r == cursor && capture != CAP_NONE)
                snprintf(val, sizeof(val), capture == CAP_ADD ? "press a button to add..." : "press a button...");
            else
                binding_str(actions[r].mask, val, sizeof(val), ", ");
            text(184, y, C_TEXT, val);
        } else if (r == ROW_STEER) {
            text(16, y, C_TEXT, "Steering (left stick)");
            text(232, y, C_TEXT, steer_invert ? "< INVERTED >" : "< NORMAL >");
        } else if (r == ROW_DEADZONE) {
            text(16, y, C_TEXT, "Stick deadzone");
            snprintf(val, sizeof(val), "< %d >", steer_deadzone);
            text(232, y, C_TEXT, val);
        } else if (r == ROW_SCREEN) {
            text(16, y, C_TEXT, "Screen");
            text(232, y, C_TEXT, stretch ? "< FULLSCREEN >" : "< ORIGINAL 3:2 >");
        } else {
            text(16, y, C_TEXT, "Restore defaults");
        }
    }
    text(16, 216, C_DIM, "X replace  [] add  /\\ unbind  <> change");
    text(16, 232, C_DIM, "O save & close   START cancel capture");
    text(16, 248, C_DIM, "Game is paused while this menu is open");
}

GLuint controls_menu_texture(void) {
    if (!menu_tex) {
        glGenTextures(1, &menu_tex);
        glBindTexture(GL_TEXTURE_2D, menu_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        dirty = 1;
    }
    if (dirty) {
        render();
        glBindTexture(GL_TEXTURE_2D, menu_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MENU_W, MENU_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, menu_px);
        dirty = 0;
    }
    return menu_tex;
}
