// SPDX-License-Identifier: MIT
/*
 * padshim: LD_PRELOAD shim for Wine processes. Exposes the Android app's
 * gamepad state (shared-memory file) to Wine's winebus.sys as an SDL virtual
 * Xbox 360 controller. winebus has only an SDL backend here (no udev/evdev),
 * and Android apps cannot read /dev/input, so this is the bridge.
 *
 * Env: PANVK_PAD_SHM = path of the 64-byte shared file (unset: shim is inert),
 *      PANVK_PAD_DEBUG = also log to stderr.
 *
 * Shared memory layout (64 bytes, little endian, writer = Android app):
 *   off  0 u32 seq          writer increments after each state write (informational)
 *   off  4 u32 connected    0: no joystick present, 1: joystick attached
 *   off  8 i16 lx, ly, rx, ry   SDL convention -32768..32767, up is negative
 *   off 16 i16 lt, rt       0..32767 (shim maps to SDL full range, rest = -32768)
 *   off 20 u16 buttons      bit = SDL button index: A0 B1 X2 Y3 BACK4 GUIDE5 START6
 *                           LSTICK7 RSTICK8 LB9 RB10 (bits 11..15 ignored, the
 *                           shim derives D-pad buttons 11..14 from the hat)
 *   off 22 u8  hat          SDL_HAT_* bits (UP1 RIGHT2 DOWN4 LEFT8)
 *   off 23 u8  reserved
 *   off 24 u16 rumble_low   written by the shim (Wine rumble request)
 *   off 26 u16 rumble_high
 *   off 28 u32 rumble_seq   shim increments on every rumble request
 *   off 32..63 reserved
 *
 * Only the process that loaded winebus.so creates the virtual joystick.
 * No SDL headers: the few types used are declared below (verified against
 * SDL 2.32.6 SDL_joystick.h, SDL_VIRTUAL_JOYSTICK_DESC_VERSION == 1).
 *
 * Update path: the app side has no futex, so this thread polls the state every
 * 4 ms and compares contents (not seq) with the last applied copy, which also
 * avoids any dependency on memory ordering of the writer. SDL_JoystickSetVirtual*
 * only stages values; Wine's own SDL event thread pumps events and thereby
 * publishes them, so this shim never calls SDL_JoystickUpdate/SDL_PumpEvents
 * (PumpEvents is not thread safe).
 */
#define _GNU_SOURCE
#include <android/log.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define EXPORT_CTOR __attribute__((constructor))

struct pad_shm {
    uint32_t seq, connected;
    int16_t lx, ly, rx, ry, lt, rt;
    uint16_t buttons;
    uint8_t hat, pad;
    uint16_t rumble_low, rumble_high;
    uint32_t rumble_seq;
    uint8_t reserved[32];
};
_Static_assert(sizeof(struct pad_shm) == 64, "shm layout");
_Static_assert(__builtin_offsetof(struct pad_shm, buttons) == 20, "shm layout");
_Static_assert(__builtin_offsetof(struct pad_shm, rumble_seq) == 28, "shm layout");

/* ---- minimal SDL2 declarations ---- */
typedef struct SDL_Joystick SDL_Joystick;
typedef struct { uint8_t data[16]; } SDL_JoystickGUID;
typedef struct SDL_VirtualJoystickDesc {
    uint16_t version, type, naxes, nbuttons, nhats, vendor_id, product_id, padding;
    uint32_t button_mask, axis_mask;
    const char *name;
    void *userdata;
    void (*Update)(void *);
    void (*SetPlayerIndex)(void *, int);
    int (*Rumble)(void *, uint16_t, uint16_t);
    int (*RumbleTriggers)(void *, uint16_t, uint16_t);
    int (*SetLED)(void *, uint8_t, uint8_t, uint8_t);
    int (*SendEffect)(void *, const void *, int);
} SDL_VirtualJoystickDesc;
#define SDL_INIT_JOYSTICK 0x00000200u
#define SDL_INIT_EVENTS 0x00004000u
#define SDL_INIT_GAMECONTROLLER 0x00002000u
#define SDL_VIRTUAL_JOYSTICK_DESC_VERSION 1
#define SDL_JOYSTICK_TYPE_GAMECONTROLLER 1
#define NBUTTONS 15

static struct {
    int (*Init)(uint32_t);
    int (*SetHint)(const char *, const char *);
    const char *(*GetError)(void);
    int (*AttachVirtualEx)(const SDL_VirtualJoystickDesc *);
    int (*DetachVirtual)(int);
    SDL_Joystick *(*JoystickOpen)(int);
    void (*JoystickClose)(SDL_Joystick *);
    int32_t (*InstanceID)(SDL_Joystick *);
    int (*NumJoysticks)(void);
    int32_t (*DeviceInstanceID)(int);
    int (*SetAxis)(SDL_Joystick *, int, int16_t);
    int (*SetButton)(SDL_Joystick *, int, uint8_t);
    int (*SetHat)(SDL_Joystick *, int, uint8_t);
    SDL_JoystickGUID (*DeviceGUID)(int);
    void (*GUIDString)(SDL_JoystickGUID, char *, int);
    int (*AddMapping)(const char *);
    int (*IsGameController)(int);
} sdl;

static struct pad_shm *g_shm;
static int g_debug;

#define LOG(prio, ...) do { \
    __android_log_print(prio, "padshim", __VA_ARGS__); \
    if (g_debug) { fprintf(stderr, "padshim: " __VA_ARGS__); fputc('\n', stderr); } \
} while (0)
#define LOGI(...) LOG(ANDROID_LOG_INFO, __VA_ARGS__)
#define LOGE(...) LOG(ANDROID_LOG_ERROR, __VA_ARGS__)

static int rumble_cb(void *ud, uint16_t low, uint16_t high)
{
    (void)ud;
    g_shm->rumble_low = low;
    g_shm->rumble_high = high;
    __atomic_add_fetch(&g_shm->rumble_seq, 1, __ATOMIC_RELEASE);
    return 0;
}

static int dl_cb(struct dl_phdr_info *info, size_t sz, void *arg)
{
    (void)sz;
    size_t n = info->dlpi_name ? strlen(info->dlpi_name) : 0;
    if (n >= 10 && !strcmp(info->dlpi_name + n - 10, "winebus.so")) {
        *(int *)arg = 1;
        return 1;
    }
    return 0;
}

static int winebus_loaded(void)
{
    int found = 0;
    dl_iterate_phdr(dl_cb, &found);
    return found;
}

static int load_sdl(void)
{
    void *h = dlopen("libSDL2-2.0.so.0", RTLD_GLOBAL | RTLD_LAZY);
    if (!h) h = dlopen("libSDL2.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!h) { LOGE("SDL2 not found: %s", dlerror()); return -1; }
#define S(f, n) do { *(void **)&sdl.f = dlsym(h, "SDL_" n); if (!sdl.f) { LOGE("missing SDL_%s", n); return -1; } } while (0)
    S(Init, "Init"); S(SetHint, "SetHint"); S(GetError, "GetError");
    S(AttachVirtualEx, "JoystickAttachVirtualEx"); S(DetachVirtual, "JoystickDetachVirtual");
    S(JoystickOpen, "JoystickOpen"); S(JoystickClose, "JoystickClose");
    S(InstanceID, "JoystickInstanceID"); S(NumJoysticks, "NumJoysticks");
    S(DeviceInstanceID, "JoystickGetDeviceInstanceID");
    S(SetAxis, "JoystickSetVirtualAxis"); S(SetButton, "JoystickSetVirtualButton");
    S(SetHat, "JoystickSetVirtualHat");
    S(DeviceGUID, "JoystickGetDeviceGUID"); S(GUIDString, "JoystickGetGUIDString");
    S(AddMapping, "GameControllerAddMapping"); S(IsGameController, "IsGameController");
#undef S
    return 0;
}

static int attach_once(void)
{
    SDL_VirtualJoystickDesc d;
    memset(&d, 0, sizeof(d));
    d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    d.naxes = 6;
    d.nbuttons = NBUTTONS;
    d.nhats = 1;
    d.vendor_id = 0x045e;
    d.product_id = 0x028e;
    d.button_mask = (1u << NBUTTONS) - 1; /* SDL_CONTROLLER_BUTTON_* A..DPAD_RIGHT */
    d.axis_mask = 0x3f;                   /* SDL_CONTROLLER_AXIS_LEFTX..TRIGGERRIGHT */
    d.name = "Xbox 360 Controller";
    d.Rumble = rumble_cb;
    return sdl.AttachVirtualEx(&d);
}

static SDL_Joystick *g_js;
static int32_t g_inst = -1;

static int attach(void)
{
    int idx = attach_once();
    if (idx < 0) { LOGE("attach failed: %s", sdl.GetError()); return -1; }
    if (!sdl.IsGameController(idx)) {
        /* Wine decides gamepad-vs-joystick when it sees the add event, so the
         * mapping must exist before that: learn the GUID, add the mapping,
         * then re-attach. Only needed if SDL has no built-in virtual mapping. */
        char guid[40], map[512];
        sdl.GUIDString(sdl.DeviceGUID(idx), guid, sizeof(guid));
        snprintf(map, sizeof(map),
                 "%s,Xbox 360 Controller,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,"
                 "leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,"
                 "dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,"
                 "leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,"
                 "platform:Android", guid);
        LOGI("adding mapping, rc=%d guid=%s", sdl.AddMapping(map), guid);
        sdl.DetachVirtual(idx);
        idx = attach_once();
        if (idx < 0) { LOGE("re-attach failed: %s", sdl.GetError()); return -1; }
    }
    g_js = sdl.JoystickOpen(idx);
    if (!g_js) { LOGE("JoystickOpen failed: %s", sdl.GetError()); sdl.DetachVirtual(idx); return -1; }
    g_inst = sdl.InstanceID(g_js);
    LOGI("virtual joystick attached idx=%d inst=%d gamecontroller=%d", idx, g_inst, sdl.IsGameController(idx));
    return 0;
}

static void detach(void)
{
    sdl.JoystickClose(g_js);
    g_js = NULL;
    for (int i = 0, n = sdl.NumJoysticks(); i < n; i++) { /* indexes shift, resolve by instance id */
        if (sdl.DeviceInstanceID(i) == g_inst) { sdl.DetachVirtual(i); break; }
    }
    LOGI("virtual joystick detached");
    g_inst = -1;
}

static void apply(const struct pad_shm *s)
{
    uint8_t hat = s->hat;
    sdl.SetAxis(g_js, 0, s->lx);
    sdl.SetAxis(g_js, 1, s->ly);
    sdl.SetAxis(g_js, 2, s->rx);
    sdl.SetAxis(g_js, 3, s->ry);
    for (int i = 0; i < 2; i++) { /* triggers: 0..32767 -> SDL -32768..32767 */
        int v = i ? s->rt : s->lt;
        v = v < 0 ? 0 : v > 32767 ? 32767 : v;
        sdl.SetAxis(g_js, 4 + i, (int16_t)(v * 65535 / 32767 - 32768));
    }
    for (int i = 0; i < 11; i++) sdl.SetButton(g_js, i, (s->buttons >> i) & 1);
    sdl.SetButton(g_js, 11, !!(hat & 1)); /* dpad up */
    sdl.SetButton(g_js, 12, !!(hat & 4)); /* down */
    sdl.SetButton(g_js, 13, !!(hat & 8)); /* left */
    sdl.SetButton(g_js, 14, !!(hat & 2)); /* right */
    sdl.SetHat(g_js, 0, hat);
}

static void msleep(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static void *worker(void *arg)
{
    (void)arg;
    while (!winebus_loaded()) msleep(200);
    if (load_sdl()) return NULL;
    sdl.SetHint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1");
    if (sdl.Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0) {
        LOGE("SDL_Init failed: %s", sdl.GetError());
        return NULL;
    }
    LOGI("winebus.so loaded, SDL ready, polling %p", (void *)g_shm);

    struct pad_shm last, cur;
    int have_last = 0, failed = 0;
    for (;;) {
        memcpy(&cur, g_shm, 32);
        int want = __atomic_load_n(&g_shm->connected, __ATOMIC_ACQUIRE) != 0;
        if (want && !g_js) {
            if (attach() < 0) { if (!failed++) LOGE("will keep retrying"); msleep(1000); continue; }
            failed = 0;
            have_last = 0;
        } else if (!want && g_js) {
            detach();
        }
        cur.seq = 0; /* seq/connected excluded from the change check */
        cur.connected = 0;
        if (g_js && (!have_last || memcmp(&cur, &last, 32))) {
            apply(&cur);
            last = cur;
            have_last = 1;
        }
        msleep(4);
    }
    return NULL;
}

EXPORT_CTOR static void padshim_init(void)
{
    const char *path = getenv("PANVK_PAD_SHM");
    if (!path || !*path) return;
    const char *dbg = getenv("PANVK_PAD_DEBUG");
    g_debug = dbg && *dbg && *dbg != '0';
    int fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) return;
    struct stat st;
    if (fstat(fd, &st) || st.st_size < (off_t)sizeof(struct pad_shm)) { close(fd); return; }
    void *m = mmap(NULL, sizeof(struct pad_shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (m == MAP_FAILED) return;
    g_shm = m;
    pthread_t t;
    if (pthread_create(&t, NULL, worker, NULL) == 0) pthread_detach(t);
}
