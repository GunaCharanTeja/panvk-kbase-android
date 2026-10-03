// SPDX-License-Identifier: MIT
/*
 * XInput probe: polls controllers 0..3 for ~20 s (XINPUT_PROBE_SECS to override),
 * printing a line whenever a state changes and "connected <idx>" when a pad first
 * appears (stdout and xinput_probe.log in the cwd). Exit 0 if any pad was seen, else 1.
 * xinput1_4.dll / xinput1_3.dll are loaded at runtime, so no import library is needed.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    WORD wButtons;
    BYTE bLeftTrigger, bRightTrigger;
    SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY;
} PAD;
typedef struct { DWORD dwPacketNumber; PAD Gamepad; } PSTATE;
typedef DWORD (WINAPI *GetStateFn)(DWORD, PSTATE *);

static FILE *g_log;

static void out(const char *fmt, ...)
{
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    fputs(b, stdout);
    fflush(stdout);
    if (g_log) { fputs(b, g_log); fflush(g_log); }
}

int main(void)
{
    const char *names[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    GetStateFn get = NULL;
    g_log = fopen("xinput_probe.log", "a");
    for (int i = 0; i < 3 && !get; i++) {
        HMODULE m = LoadLibraryA(names[i]);
        if (m) {
            get = (GetStateFn)(void *)GetProcAddress(m, "XInputGetState");
            if (get) out("using %s\n", names[i]);
        }
    }
    if (!get) { out("no xinput dll\n"); return 1; }

    const char *s = getenv("XINPUT_PROBE_SECS");
    DWORD secs = s ? (DWORD)atoi(s) : 20;
    DWORD t0 = GetTickCount();
    int ever = 0, up[4] = { 0 };
    PSTATE last[4];
    memset(last, 0, sizeof last);
    while (GetTickCount() - t0 < secs * 1000) {
        for (DWORD i = 0; i < 4; i++) {
            PSTATE st;
            memset(&st, 0, sizeof st);
            if (get(i, &st) != ERROR_SUCCESS) {
                if (up[i]) { out("disconnected %lu\n", (unsigned long)i); up[i] = 0; }
                continue;
            }
            if (!up[i]) { out("connected %lu\n", (unsigned long)i); up[i] = 1; ever = 1; last[i].dwPacketNumber = ~st.dwPacketNumber; }
            if (st.dwPacketNumber != last[i].dwPacketNumber || memcmp(&st.Gamepad, &last[i].Gamepad, sizeof st.Gamepad)) {
                out("pad%lu pkt=%lu btn=0x%04x L=(%d,%d) R=(%d,%d) LT=%u RT=%u\n", (unsigned long)i,
                    (unsigned long)st.dwPacketNumber, st.Gamepad.wButtons, st.Gamepad.sThumbLX,
                    st.Gamepad.sThumbLY, st.Gamepad.sThumbRX, st.Gamepad.sThumbRY,
                    st.Gamepad.bLeftTrigger, st.Gamepad.bRightTrigger);
                last[i] = st;
            }
        }
        Sleep(10);
    }
    out("done, %s\n", ever ? "controller seen" : "no controller");
    if (g_log) fclose(g_log);
    return ever ? 0 : 1;
}
