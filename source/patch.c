/**
 * @file patch.c
 * @brief Game-specific hooks, memory patches, and custom symbol intercepts for Crash Nitro Kart 3D.
 */
#include <sys/stat.h> // antes que psp2/io/stat.h (hosttest: macros st_*time de glibc)
#include <fcntl.h>
#include <math.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/stat.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "controls.h"
#include "darwin.h"
#include "gles.h"
#include "hle.h"
#include "imports.h"
#include "input.h"
#include "log.h"
#include "macho.h"
#include "patch.h"

// C++ allocation & exception personality stubs
static void *cpp_new(size_t sz) { return malloc(sz); }
static void cpp_delete(void *p) { free(p); }
static int dummy_personality(void) { return 0; }

// POSIX file descriptor shims
static int p_open(const char *path, int flags, int mode) {
    char p[512];
    int fd = open(darwin_path(path, p, sizeof(p)), flags, mode);
    l_debug("open(%s, 0x%x) -> %d", p, flags, fd);
    return fd;
}

// struct stat de iOS 32 bits (ino de 64 bits, 108 bytes): PFile::Size() lee st_size en +60.
// La de newlib tiene otra disposición, así que se traduce a mano.
static int p_fstat(int fd, uint8_t *dst) {
    struct stat st;
    int r = fstat(fd, &st);
    if (r < 0)
        return r;
    memset(dst, 0, 108);
    *(uint16_t *)(dst + 4) = (uint16_t)st.st_mode;
    *(uint16_t *)(dst + 6) = 1;                      // st_nlink
    int64_t size = (int64_t)st.st_size;
    memcpy(dst + 60, &size, 8);                      // st_size (off_t, 64 bits)
    *(int32_t *)(dst + 84) = 512;                    // st_blksize
    return 0;
}

// Darwin: off_t de 64 bits; el ABI de iOS no alinea el par, así que llega fd=r0,
// offset=r1:r2, whence=r3, y el resultado vuelve en r0:r1.
static int64_t p_lseek(int fd, uint32_t off_lo, int32_t off_hi, int whence) {
    int64_t off = (int64_t)(((uint64_t)(uint32_t)off_hi << 32) | off_lo);
    int64_t r = (int64_t)lseek(fd, (off_t)off, whence);
    l_debug("lseek(%d, %lld, %d) -> %lld", fd, (long long)off, whence, (long long)r);
    return r;
}

static ssize_t p_read(int fd, void *buf, size_t count) {
    ssize_t r = read(fd, buf, count);
    l_debug("read(%d, %u) -> %d", fd, (unsigned)count, (int)r);
    return r;
}

static ssize_t p_write(int fd, const void *buf, size_t count) {
    return write(fd, buf, count);
}

static int p_remove(const char *path) {
    char p[512];
    return remove(darwin_path(path, p, sizeof(p)));
}

static char *p_getenv(const char *name) {
    return getenv(name);
}

static int p_setenv(const char *name, const char *val, int overwrite) {
    return setenv(name, val, overwrite);
}

static int p_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    int r = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    l_info("[GUEST PRINTF] %s", buf);
    return r;
}

static int p_fprintf(FILE *stream, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    int r = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    l_info("[GUEST FPRINTF] %s", buf);
    return r;
}

static int game_started = 0;

void patch_start_game(void) {
    if (game_started)
        return;
    game_started = 1;

    uintptr_t fn_init = macho_sym("__Z9game_initv");
    if (!fn_init)
        fn_init = macho_sym("_fuse_thread");

    if (fn_init) {
        l_info("Invoking Polarbit engine init at 0x%08X...", (unsigned)fn_init);
        int res = call_guest((void *)fn_init, 0, 0, 0, 0);
        l_info("Polarbit engine init result=%d", res);
        // game_init: 0 = ok, 2 = PGameSystemManager::Init falló (y ya se destruyó). En iOS
        // _fuse_thread llama exit(0); aquí se avisa en pantalla en vez de quedar en negro.
        if (res != 0)
            fatal_error("Game engine init failed (code %d). See %s", res, LOGS_PATH);
    } else {
        l_error("game_init / _fuse_thread symbol not found in Mach-O!");
    }
}

static int p_UIApplicationMain(int argc, char *argv[], void *principalClassName, void *delegateClassName) {
    l_info("UIApplicationMain called (argc=%d)", argc);
    patch_start_game();
    return 0;
}

// Game-specific import tables for Crash Nitro Kart 3D
const import_entry patch_imports[] = {
    {"_open", (uintptr_t)p_open, 0},
    {"_read", (uintptr_t)p_read, 0},
    {"_write", (uintptr_t)p_write, 0},
    {"_lseek", (uintptr_t)p_lseek, 0},
    {"_fstat", (uintptr_t)p_fstat, 0},
    {"_remove", (uintptr_t)p_remove, 0},
    {"_getenv", (uintptr_t)p_getenv, 0},
    {"_setenv", (uintptr_t)p_setenv, 0},
    {"_printf", (uintptr_t)p_printf, 0},
    {"_fprintf", (uintptr_t)p_fprintf, 0},
    {"__Znwm", (uintptr_t)cpp_new, 0},
    {"__Znam", (uintptr_t)cpp_new, 0},
    {"__ZdaPv", (uintptr_t)cpp_delete, 0},
    {"___gxx_personality_sj0", (uintptr_t)dummy_personality, 0},
    {"_UIApplicationMain", (uintptr_t)p_UIApplicationMain, 0},
};
const int patch_imports_count = sizeof(patch_imports) / sizeof(patch_imports[0]);

void patch_init(void) {
    l_info("Initializing Polarbit Fuse engine patches for Crash Nitro Kart...");

    // Configure Polarbit engine environment paths
    setenv("FUSEAPPPATH", BUNDLE_PATH, 1);
    setenv("FUSEAPP_SAVEPATH", DOCS_PATH, 1);
    sceIoMkdir(DATA_PATH, 0777);
    sceIoMkdir(DOCS_PATH, 0777);
    controls_init();
}

void patch_frame(void) {
    if (!game_started)
        patch_start_game();

    static uintptr_t fn_game_run = 0;
    static uintptr_t fn_set_joy = 0;
    static uintptr_t fn_on_event = 0;
    static int symbols_cached = 0;

    if (!symbols_cached) {
        fn_game_run = macho_sym("_game_run");
        fn_set_joy = macho_sym("_SetJoystickValues");
        fn_on_event = macho_sym("_OnEvent");
        symbols_cached = 1;
    }

    // 0. Menú de controles (START+SELECT): pausa el juego y suelta los toques virtuales.
    if (controls_menu_update(pad.buttons)) {
        for (int i = 0; i < ACT_COUNT; i++)
            if (actions[i].down && fn_on_event) {
                actions[i].down = 0;
                call_guest((void *)fn_on_event, actions[i].x, actions[i].y, 1, 0);
            }
        gles_overlay_tex = controls_menu_texture();
        return;
    }
    gles_overlay_tex = 0;

    // 1. Steering. El juego dirige por inclinación: -[iFuseAppDelegate accelerometer:didAccelerate:]
    //    llama SetJoystickValues(ax*32768, ay*32768, az*32768) con la gravedad en g. Se simula el
    //    iPhone en horizontal (gravedad en -x), inclinado STEER_TILT hacia atrás y girado como un
    //    volante según el stick izquierdo.
    //    Signo calibrado en consola (corrida 008: con +1 iba al revés); steer_invert lo vuelve a girar.
    if (fn_set_joy) {
        const float STEER_MAX = 0.6f /* rad, ~35° */, STEER_TILT = 0.52f /* ~30° */;
        int dx = (int)pad.lx - 128;
        if (dx > -steer_deadzone && dx < steer_deadzone)
            dx = 0;
        float th = (steer_invert ? 1.0f : -1.0f) * STEER_MAX * (float)dx / 128.0f;
        float ax = -cosf(th) * cosf(STEER_TILT), ay = sinf(th) * cosf(STEER_TILT), az = -sinf(STEER_TILT);
        call_guest((void *)fn_set_joy, (int)(ax * 32768.0f), (int)(ay * 32768.0f), (int)(az * 32768.0f), 0);
    }

    // 2. Touch & buttons via OnEvent(x, y, 1, code) en coordenadas horizontales 480x320,
    //    como -[EAGLView touches*]: code 1 = began, 2 = moved, 0 = ended.
    if (fn_on_event) {
        static touch_point prev[SCE_TOUCH_MAX_REPORT];
        static int nprev;
        for (int i = 0; i < touch_count; i++) {
            int moved = 0;
            for (int j = 0; j < nprev; j++)
                if (prev[j].id == touches[i].id)
                    moved = 1;
            call_guest((void *)fn_on_event, touches[i].gx, touches[i].gy, 1, moved ? 2 : 1);
        }
        for (int j = 0; j < nprev; j++) {
            int still = 0;
            for (int i = 0; i < touch_count; i++)
                if (touches[i].id == prev[j].id)
                    still = 1;
            if (!still)
                call_guest((void *)fn_on_event, prev[j].gx, prev[j].gy, 1, 0);
        }
        memcpy(prev, touches, sizeof(prev));
        nprev = touch_count;

        // Botones físicos -> toques virtuales (config.txt).
        for (int i = 0; i < ACT_COUNT; i++) {
            int pressed = (pad.buttons & actions[i].mask) != 0;
            if (pressed != actions[i].down) {
                actions[i].down = pressed;
                call_guest((void *)fn_on_event, actions[i].x, actions[i].y, 1, pressed ? 1 : 0);
            }
        }
    }

    // 3. Render and advance game frame
    if (fn_game_run) {
        static uint32_t frames, last_draws;
        static uint64_t run_us, last_t;
        uint64_t t0 = sceKernelGetProcessTimeWide();
        int r = call_guest((void *)fn_game_run, 0, 0, 0, 0);
        run_us += sceKernelGetProcessTimeWide() - t0;
        // Latido: distingue cuelgue dentro de game_run / bucle sin dibujar / juego corriendo.
        if (++frames == 1 || frames % 300 == 0) {
            uint32_t d = gles_get_draw_calls();
            uint64_t now = sceKernelGetProcessTimeWide();
            l_info("frame %u: game_run=%d draws=+%u avg %u us fps=%u", frames, r, d - last_draws,
                   (unsigned)(run_us / (frames == 1 ? 1 : 300)),
                   frames == 1 ? 0 : (unsigned)(300ULL * 1000000 / (now - last_t)));
            last_draws = d;
            last_t = now;
            run_us = 0;
        }
        if (r == 2) { // el motor ya se destruyó; en iOS runFrame llama exit(0)
            l_info("Game requested exit");
            sceKernelExitProcess(0);
        }
    }
}
