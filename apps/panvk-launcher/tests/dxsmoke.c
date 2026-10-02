/*
 * Windowed clear/present smoke for one of d3d8, d3d9, d3d10, d3d11.
 * Every graphics entry point is LoadLibrary/GetProcAddress'd, so the
 * binary links against nothing but the CRT (no d3d import libs).
 *
 *   dxsmoke.exe <d3d8|d3d9|d3d10|d3d11>
 *
 * Creates a hardware device on a visible window, clears the back buffer
 * to opaque orange, presents (eight frames for d3d11), logs HRESULTs and
 * adapter string, then exits. Readback and extra sync are opt-in diagnostics.
 * A watchdog bounds execution to the hold interval plus eight seconds.
 *
 * Built by build-dxsmoke.sh. Not a driver test.
 */
#define COBJMACROS
#define CINTERFACE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* d3d8types.h and d3d9types.h both define the same enums, so the d3d8
 * path lives in its own translation unit (dxsmoke_d3d8.c). */
void run_d3d8(void);
void done(int code);
void fail(const char *what, HRESULT hr);
void *sym(HMODULE mod, const char *name);
HWND make_window(const char *title);
void pump(DWORD ms);
DWORD hold_ms(void);
int staging_enabled(void);

/* headers only declare these GUIDs; define the four this file takes the
 * address of so nothing has to be pulled out of uuid.lib / dxguid.lib */
const GUID IID_IUnknown = {0x00000000,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
const GUID IID_IDXGIDevice = {0x54ec77fa,0x1377,0x44e6,{0x8c,0x32,0x88,0xfd,0x5f,0x44,0xc8,0x4c}};
const GUID IID_ID3D10Texture2D = {0x9b7e4c04,0x342c,0x4106,{0xa1,0x9f,0x4f,0x27,0x04,0xf6,0x89,0xf0}};
const GUID IID_ID3D11Texture2D = {0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};

#define W 320
#define H 240
#define DEADLINE_PAD_MS 8000
#define HOLD_DEFAULT_MS 750
#define HOLD_MIN_MS 200
#define HOLD_MAX_MS 20000

static volatile LONG g_done;

/* Test harness only. Unset keeps the old 750ms present window.
 * Not read from the launcher's process environment. */
DWORD hold_ms(void)
{
    const char *e = getenv("DXSMOKE_HOLD");
    char *end = NULL;
    unsigned long v;

    if (!e || !*e)
        return HOLD_DEFAULT_MS;
    v = strtoul(e, &end, 10);
    if (end == e || v < HOLD_MIN_MS)
        v = HOLD_MIN_MS;
    if (v > HOLD_MAX_MS)
        v = HOLD_MAX_MS;
    return (DWORD)v;
}

static DWORD WINAPI watchdog(void *unused)
{
    DWORD deadline = hold_ms() + DEADLINE_PAD_MS;

    (void)unused;
    Sleep(deadline);
    if (!g_done) {
        printf("DXSMOKE: FAIL deadline %lums exceeded\n", (unsigned long)deadline);
        fflush(stdout);
        ExitProcess(3);
    }
    return 0;
}

void done(int code)
{
    InterlockedExchange(&g_done, 1);
    fflush(stdout);
    ExitProcess(code);
}

void fail(const char *what, HRESULT hr)
{
    printf("DXSMOKE: FAIL %s hr=0x%08lx\n", what, (unsigned long)hr);
    done(1);
}

void *sym(HMODULE mod, const char *name)
{
    void *p = (void *)GetProcAddress(mod, name);
    if (!p)
        fail(name, HRESULT_FROM_WIN32(GetLastError()));
    return p;
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_CLOSE || msg == WM_DESTROY)
        return 0;
    return DefWindowProcA(hwnd, msg, wp, lp);
}

HWND make_window(const char *title)
{
    WNDCLASSA wc;
    HWND hwnd;
    RECT r = {0, 0, W, H};

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "dxsmoke";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        fail("RegisterClassA", HRESULT_FROM_WIN32(GetLastError()));

    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW | WS_VISIBLE, FALSE);
    hwnd = CreateWindowA("dxsmoke", title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                          CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                          NULL, NULL, wc.hInstance, NULL);
    if (!hwnd)
        fail("CreateWindowA", HRESULT_FROM_WIN32(GetLastError()));
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    {
        RECT cr;
        BOOL vis = IsWindowVisible(hwnd);

        memset(&cr, 0, sizeof(cr));
        GetClientRect(hwnd, &cr);
        printf("DXSMOKE: window hwnd=%p visible=%d client=%ldx%ld hold_ms=%lu\n",
               (void *)hwnd, vis ? 1 : 0,
               (long)(cr.right - cr.left), (long)(cr.bottom - cr.top),
               (unsigned long)hold_ms());
    }
    return hwnd;
}

/* pump just long enough for the present to land on screen */
void pump(DWORD ms)
{
    MSG msg;
    DWORD t = GetTickCount();
    while (GetTickCount() - t < ms) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        Sleep(15);
    }
}

/* ---------------------------------------------------------------- d3d9 */

void source_pixels(const char *api, const void *data, unsigned pitch)
{
    unsigned orange = 0, zero = 0, other = 0;
    for (unsigned y = 0; y < H; y++) {
        const unsigned char *row = (const unsigned char *)data + (size_t)y * pitch;
        for (unsigned x = 0; x < W; x++) {
            const unsigned char *p = row + x * 4;
            if (p[0] == 64 && p[1] == 128 && p[2] == 255) orange++;
            else if (!p[0] && !p[1] && !p[2]) zero++;
            else other++;
        }
    }
    printf("DXSMOKE: api=%s source orange=%u zero=%u other=%u need=%u\n",
           api, orange, zero, other, W * H);
    if (orange != W * H) fail("source pixels not orange", E_FAIL);
}

static void run_d3d9(void)
{
    HMODULE dll = LoadLibraryA("d3d9.dll");
    IDirect3D9 *(WINAPI *create)(UINT);
    IDirect3D9 *d3d;
    IDirect3DDevice9 *dev = NULL;
    IDirect3DSurface9 *bb = NULL, *st = NULL;
    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT map;
    D3DPRESENT_PARAMETERS pp;
    D3DADAPTER_IDENTIFIER9 id;
    HRESULT hr;
    HWND hwnd;

    if (!dll)
        fail("LoadLibrary(d3d9.dll)", HRESULT_FROM_WIN32(GetLastError()));
    create = sym(dll, "Direct3DCreate9");
    d3d = create(D3D_SDK_VERSION);
    if (!d3d)
        fail("Direct3DCreate9", E_FAIL);

    hwnd = make_window("dxsmoke d3d9");
    memset(&pp, 0, sizeof(pp));
    pp.BackBufferWidth = W;
    pp.BackBufferHeight = H;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = hwnd;
    pp.Windowed = TRUE;

    hr = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                 D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
    printf("DXSMOKE: api=d3d9 CreateDevice hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr) || !dev)
        fail("IDirect3D9::CreateDevice", hr);

    memset(&id, 0, sizeof(id));
    hr = IDirect3D9_GetAdapterIdentifier(d3d, D3DADAPTER_DEFAULT, 0, &id);
    printf("DXSMOKE: api=d3d9 adapter hr=0x%08lx desc=\"%s\" vendor=0x%04lx device=0x%04lx\n",
           (unsigned long)hr, id.Description, (unsigned long)id.VendorId, (unsigned long)id.DeviceId);

    hr = IDirect3DDevice9_Clear(dev, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 128, 64), 1.0f, 0);
    printf("DXSMOKE: api=d3d9 Clear hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr))
        fail("IDirect3DDevice9::Clear", hr);
    if (staging_enabled()) {
        hr = IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
        if (FAILED(hr)) fail("GetBackBuffer", hr);
        hr = IDirect3DSurface9_GetDesc(bb, &desc);
        if (FAILED(hr)) fail("GetDesc", hr);
        hr = IDirect3DDevice9_CreateOffscreenPlainSurface(dev, W, H, desc.Format, D3DPOOL_SYSTEMMEM, &st, NULL);
        if (FAILED(hr)) fail("CreateOffscreenPlainSurface", hr);
        hr = IDirect3DDevice9_GetRenderTargetData(dev, bb, st);
        if (FAILED(hr)) fail("GetRenderTargetData", hr);
        hr = IDirect3DSurface9_LockRect(st, &map, NULL, D3DLOCK_READONLY);
        if (FAILED(hr)) fail("LockRect", hr);
        source_pixels("d3d9", map.pBits, map.Pitch);
        IDirect3DSurface9_UnlockRect(st);
        IDirect3DSurface9_Release(st);
        IDirect3DSurface9_Release(bb);
    }
    hr = IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL);
    printf("DXSMOKE: api=d3d9 Present hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr))
        fail("IDirect3DDevice9::Present", hr);

    pump(hold_ms());
    IDirect3DDevice9_Release(dev);
    IDirect3D9_Release(d3d);
    printf("DXSMOKE: PASS api=d3d9\n");
    done(0);
}

/* ------------------------------------------------------- d3d10 / d3d11 */

static void log_adapter(IDXGISwapChain *sc, const char *api)
{
    IDXGIDevice *dxgi = NULL;
    IDXGIAdapter *adapter = NULL;
    DXGI_ADAPTER_DESC desc;
    char name[256];
    HRESULT hr;
    IUnknown *dev = NULL;

    hr = IDXGISwapChain_GetDevice(sc, &IID_IUnknown, (void **)&dev);
    if (FAILED(hr) || !dev) {
        printf("DXSMOKE: api=%s adapter hr=0x%08lx (GetDevice)\n", api, (unsigned long)hr);
        return;
    }
    hr = IUnknown_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dxgi);
    IUnknown_Release(dev);
    if (FAILED(hr) || !dxgi) {
        printf("DXSMOKE: api=%s adapter hr=0x%08lx (IDXGIDevice)\n", api, (unsigned long)hr);
        return;
    }
    hr = IDXGIDevice_GetAdapter(dxgi, &adapter);
    IDXGIDevice_Release(dxgi);
    if (FAILED(hr) || !adapter) {
        printf("DXSMOKE: api=%s adapter hr=0x%08lx (GetAdapter)\n", api, (unsigned long)hr);
        return;
    }
    memset(&desc, 0, sizeof(desc));
    hr = IDXGIAdapter_GetDesc(adapter, &desc);
    IDXGIAdapter_Release(adapter);
    name[0] = 0;
    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), NULL, NULL);
    printf("DXSMOKE: api=%s adapter hr=0x%08lx desc=\"%s\" vendor=0x%04x device=0x%04x\n",
           api, (unsigned long)hr, name, (unsigned)desc.VendorId, (unsigned)desc.DeviceId);
}

static void fill_sc(DXGI_SWAP_CHAIN_DESC *sd, HWND hwnd)
{
    memset(sd, 0, sizeof(*sd));
    sd->BufferDesc.Width = W;
    sd->BufferDesc.Height = H;
    sd->BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd->SampleDesc.Count = 1;
    sd->BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd->BufferCount = 1;
    sd->OutputWindow = hwnd;
    sd->Windowed = TRUE;
    sd->SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
}

static void run_d3d10(void)
{
    HMODULE dll = LoadLibraryA("d3d10.dll");
    HRESULT (WINAPI *create)(IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT,
                             DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **, ID3D10Device **);
    IDXGISwapChain *sc = NULL;
    ID3D10Device *dev = NULL;
    ID3D10Texture2D *bb = NULL;
    ID3D10Texture2D *st = NULL;
    D3D10_TEXTURE2D_DESC td;
    D3D10_MAPPED_TEXTURE2D map;
    ID3D10RenderTargetView *rtv = NULL;
    DXGI_SWAP_CHAIN_DESC sd;
    const float color[4] = {1.0f, 0.5f, 0.25f, 1.0f};
    HRESULT hr;
    HWND hwnd;

    if (!dll)
        fail("LoadLibrary(d3d10.dll)", HRESULT_FROM_WIN32(GetLastError()));
    create = sym(dll, "D3D10CreateDeviceAndSwapChain");

    hwnd = make_window("dxsmoke d3d10");
    fill_sc(&sd, hwnd);
    hr = create(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_SDK_VERSION, &sd, &sc, &dev);
    printf("DXSMOKE: api=d3d10 D3D10CreateDeviceAndSwapChain hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr) || !dev || !sc)
        fail("D3D10CreateDeviceAndSwapChain", hr);
    log_adapter(sc, "d3d10");

    hr = IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D10Texture2D, (void **)&bb);
    if (FAILED(hr) || !bb)
        fail("IDXGISwapChain::GetBuffer", hr);
    hr = ID3D10Device_CreateRenderTargetView(dev, (ID3D10Resource *)bb, NULL, &rtv);
    if (FAILED(hr) || !rtv)
        fail("ID3D10Device::CreateRenderTargetView", hr);

    ID3D10Device_OMSetRenderTargets(dev, 1, &rtv, NULL);
    ID3D10Device_ClearRenderTargetView(dev, rtv, color);
    printf("DXSMOKE: api=d3d10 Clear issued\n");
    if (staging_enabled()) {
        ID3D10Texture2D_GetDesc(bb, &td);
        td.Usage = D3D10_USAGE_STAGING;
        td.BindFlags = 0;
        td.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
        td.MiscFlags = 0;
        hr = ID3D10Device_CreateTexture2D(dev, &td, NULL, &st);
        if (FAILED(hr)) fail("CreateTexture2D staging", hr);
        ID3D10Device_CopyResource(dev, (ID3D10Resource *)st, (ID3D10Resource *)bb);
        hr = ID3D10Texture2D_Map(st, 0, D3D10_MAP_READ, 0, &map);
        if (FAILED(hr)) fail("Map staging", hr);
        source_pixels("d3d10", map.pData, map.RowPitch);
        ID3D10Texture2D_Unmap(st, 0);
        ID3D10Texture2D_Release(st);
    }
    ID3D10Texture2D_Release(bb);
    hr = IDXGISwapChain_Present(sc, 0, 0);
    printf("DXSMOKE: api=d3d10 Present hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr))
        fail("IDXGISwapChain::Present", hr);

    pump(hold_ms());
    ID3D10RenderTargetView_Release(rtv);
    IDXGISwapChain_Release(sc);
    ID3D10Device_Release(dev);
    printf("DXSMOKE: PASS api=d3d10\n");
    done(0);
}

int staging_enabled(void)
{
    const char *e = getenv("DXSMOKE_STAGING");
    if (!e || !*e)
        return 0;
    return atoi(e) > 0 || !strcmp(e, "true") || !strcmp(e, "yes") || !strcmp(e, "on");
}

/* Orange clear is RGBA float {1, 0.5, 0.25, 1} -> B8G8R8A8 bytes 40 80 ff ff.
 * ORANGE_PASS: every pixel matches and alpha is 0xff on formats that have it.
 * API success is a separate line. ponytail: histogram, not a window photo. */
static int readback_bb(ID3D11Device *dev, ID3D11DeviceContext *ctx, const char *tag,
                       unsigned *out_orange, unsigned *out_zero, unsigned *out_other)
{
    ID3D11Texture2D *bb = NULL, *st = NULL;
    D3D11_TEXTURE2D_DESC td, sd;
    D3D11_MAPPED_SUBRESOURCE map;
    HRESULT hr;
    unsigned orange = 0, zero = 0, other = 0, alpha_bad = 0, x, y;
    const unsigned char *row;
    unsigned char r, g, b, a;
    int is_bgra;

    if (out_orange) *out_orange = 0;
    if (out_zero) *out_zero = 0;
    if (out_other) *out_other = 0;

    hr = ID3D11Device_GetDeviceRemovedReason(dev);
    printf("DXSMOKE: api=d3d11 %s removed=0x%08lx\n", tag, (unsigned long)hr);
    if (FAILED(hr) || hr != S_OK)
        fail("ID3D11Device_GetDeviceRemovedReason (device removed pre-readback)", hr);

    /* GetBuffer lives on the swapchain; caller passes the live back buffer via OM. */
    {
        ID3D11RenderTargetView *bound = NULL;
        ID3D11Resource *res = NULL;

        ID3D11DeviceContext_OMGetRenderTargets(ctx, 1, &bound, NULL);
        if (!bound) {
            printf("DXSMOKE: api=d3d11 %s RTV null\n", tag);
            return -1;
        }
        ID3D11RenderTargetView_GetResource(bound, &res);
        ID3D11RenderTargetView_Release(bound);
        if (!res) {
            printf("DXSMOKE: api=d3d11 %s RTV resource null\n", tag);
            return -1;
        }
        hr = ID3D11Resource_QueryInterface(res, &IID_ID3D11Texture2D, (void **)&bb);
        ID3D11Resource_Release(res);
        if (FAILED(hr) || !bb) {
            printf("DXSMOKE: api=d3d11 %s backbuffer QI hr=0x%08lx\n", tag, (unsigned long)hr);
            return -1;
        }
    }

    memset(&td, 0, sizeof(td));
    ID3D11Texture2D_GetDesc(bb, &td);
    if (tag[3] == '0') {
        UINT support = 0;
        HRESULT sh = ID3D11Device_CheckFormatSupport(dev, td.Format, &support);
        printf("DXSMOKE: api=d3d11 %s bb %lux%lu fmt=%u bind=0x%x usage=%u samples=%u mips=%u support_hr=0x%08lx support=0x%x\n",
               tag, (unsigned long)td.Width, (unsigned long)td.Height, (unsigned)td.Format,
               (unsigned)td.BindFlags, (unsigned)td.Usage, (unsigned)td.SampleDesc.Count,
               (unsigned)td.MipLevels, (unsigned long)sh, (unsigned)support);
    }

    /* Sol review: staging format MUST match backbuffer format for CopyResource */
    memset(&sd, 0, sizeof(sd));
    sd.Width = td.Width ? td.Width : 64;
    sd.Height = td.Height ? td.Height : 64;
    sd.MipLevels = 1;
    sd.ArraySize = 1;
    sd.Format = td.Format;
    sd.SampleDesc.Count = 1;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = ID3D11Device_CreateTexture2D(dev, &sd, NULL, &st);
    if (FAILED(hr) || !st) {
        printf("DXSMOKE: api=d3d11 %s bb staging CreateTexture2D hr=0x%08lx wh=%lux%lu fmt=%u\n",
               tag, (unsigned long)hr, (unsigned long)sd.Width, (unsigned long)sd.Height, (unsigned)sd.Format);
        ID3D11Texture2D_Release(bb);
        return -2;
    }

    ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)st, (ID3D11Resource *)bb);
    ID3D11DeviceContext_Flush(ctx);
    memset(&map, 0, sizeof(map));
    hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &map);
    printf("DXSMOKE: api=d3d11 %s Map hr=0x%08lx pitch=%u\n",
           tag, (unsigned long)hr, (unsigned)map.RowPitch);
    if (FAILED(hr) || !map.pData) {
        ID3D11Texture2D_Release(st);
        ID3D11Texture2D_Release(bb);
        return -3;
    }

    is_bgra = (td.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
               td.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ||
               td.Format == DXGI_FORMAT_B8G8R8X8_UNORM ||
               td.Format == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB);

    for (y = 0; y < td.Height; y++) {
        row = (const unsigned char *)map.pData + (size_t)y * map.RowPitch;
        for (x = 0; x < td.Width; x++) {
            const unsigned char *p = row + x * 4u;
            unsigned char pr, pg, pb;
            if (is_bgra) {
                pb = p[0]; pg = p[1]; pr = p[2];
            } else {
                pr = p[0]; pg = p[1]; pb = p[2];
            }
            int hit = abs((int)pr - 0xff) <= 1 && abs((int)pg - 0x80) <= 1 && abs((int)pb - 0x40) <= 1;
            int z = (pr == 0 && pg == 0 && pb == 0);
            int has_alpha = td.Format != DXGI_FORMAT_B8G8R8X8_UNORM &&
                            td.Format != DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
            if (hit && has_alpha && p[3] != 0xff)
                alpha_bad++;
            else if (hit)
                orange++;
            else if (z)
                zero++;
            else
                other++;
        }
    }
    row = (const unsigned char *)map.pData + ((size_t)td.Height / 2u) * map.RowPitch;
    row += (td.Width / 2u) * 4u;
    if (is_bgra) {
        b = row[0]; g = row[1]; r = row[2]; a = row[3];
    } else {
        r = row[0]; g = row[1]; b = row[2]; a = row[3];
    }
    printf("DXSMOKE: api=d3d11 %s center RGBA=%02x %02x %02x %02x (raw=%02x %02x %02x %02x fmt=%u) orange=%u zero=%u other=%u alpha_bad=%u need=%u\n",
           tag, r, g, b, a, row[0], row[1], row[2], row[3], (unsigned)td.Format,
           orange, zero, other, alpha_bad, td.Width * td.Height);

    ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)st, 0);
    ID3D11Texture2D_Release(st);
    ID3D11Texture2D_Release(bb);

    if (out_orange) *out_orange = orange;
    if (out_zero) *out_zero = zero;
    if (out_other) *out_other = other;
    return (td.Width && td.Height && orange == td.Width * td.Height &&
            zero == 0 && other == 0 && alpha_bad == 0) ? 1 : 0;
}

static void run_d3d11(void)
{
    HMODULE dll = LoadLibraryA("d3d11.dll");
    HRESULT (WINAPI *create)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT,
                             const D3D_FEATURE_LEVEL *, UINT, UINT, const DXGI_SWAP_CHAIN_DESC *,
                             IDXGISwapChain **, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
    IDXGISwapChain *sc = NULL;
    ID3D11Device *dev = NULL;
    ID3D11DeviceContext *ctx = NULL;
    ID3D11Texture2D *bb = NULL;
    ID3D11RenderTargetView *rtv = NULL;
    DXGI_SWAP_CHAIN_DESC sd;
    D3D_FEATURE_LEVEL fl = 0;
    const float color[4] = {1.0f, 0.5f, 0.25f, 1.0f};
    HRESULT hr;
    HWND hwnd;
    unsigned frame, nframes = 8;
    const char *sync = getenv("DXSMOKE_SYNC");
    ID3D11Query *event = NULL;
    int staging_on = staging_enabled();
    int last_rb = -99, fail_create = 0, fail_map = 0, frames_ok = 0;
    unsigned tot_orange = 0, tot_zero = 0, tot_other = 0;

    if (!dll)
        fail("LoadLibrary(d3d11.dll)", HRESULT_FROM_WIN32(GetLastError()));
    create = sym(dll, "D3D11CreateDeviceAndSwapChain");

    hwnd = make_window("dxsmoke d3d11");
    fill_sc(&sd, hwnd);
    hr = create(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION,
                &sd, &sc, &dev, &fl, &ctx);
    printf("DXSMOKE: api=d3d11 D3D11CreateDeviceAndSwapChain hr=0x%08lx feature=0x%x\n",
           (unsigned long)hr, (unsigned)fl);
    if (FAILED(hr) || !dev || !sc || !ctx)
        fail("D3D11CreateDeviceAndSwapChain", hr);
    log_adapter(sc, "d3d11");
    if (sync && strcmp(sync, "flush") && strcmp(sync, "event"))
        fail("DXSMOKE_SYNC must be flush or event", E_INVALIDARG);
    if (sync && !strcmp(sync, "event")) {
        D3D11_QUERY_DESC qd = {D3D11_QUERY_EVENT, 0};
        hr = ID3D11Device_CreateQuery(dev, &qd, &event);
        if (FAILED(hr) || !event) fail("CreateQuery event", hr);
    }
    printf("DXSMOKE: diagnostic sync=%s staging=%d\n", sync ? sync : "none", staging_on);

    hr = IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
    if (FAILED(hr) || !bb)
        fail("IDXGISwapChain::GetBuffer", hr);
    hr = ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv);
    ID3D11Texture2D_Release(bb);
    if (FAILED(hr) || !rtv)
        fail("ID3D11Device::CreateRenderTargetView", hr);

    ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL);
    for (frame = 0; frame < nframes; frame++) {
        char tag[32];
        unsigned fo = 0, fz = 0, fot = 0;

        ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, color);
        if (event) {
            BOOL complete = FALSE;
            DWORD start = GetTickCount();
            ID3D11DeviceContext_End(ctx, (ID3D11Asynchronous *)event);
            ID3D11DeviceContext_Flush(ctx);
            do {
                hr = ID3D11DeviceContext_GetData(ctx, (ID3D11Asynchronous *)event,
                                                &complete, sizeof(complete), 0);
                if (FAILED(hr)) fail("GetData event", hr);
                if (GetTickCount() - start > 5000) fail("GetData event timeout", E_FAIL);
                if (hr == S_FALSE) Sleep(1);
            } while (hr == S_FALSE);
            if (!complete) fail("GetData event incomplete", E_FAIL);
            printf("DXSMOKE: GPU event complete frame=%u elapsed_ms=%lu\n",
                   frame, (unsigned long)(GetTickCount() - start));
        } else if (sync) {
            ID3D11DeviceContext_Flush(ctx);
        }
        snprintf(tag, sizeof(tag), "pre%u", frame);
        printf("DXSMOKE: api=d3d11 Clear issued frame=%u\n", frame);

        hr = ID3D11Device_GetDeviceRemovedReason(dev);
        if (FAILED(hr) || hr != S_OK)
            fail("ID3D11Device_GetDeviceRemovedReason (device removed pre-present)", hr);

        if (staging_on) {
            last_rb = readback_bb(dev, ctx, tag, &fo, &fz, &fot);
            tot_orange += fo;
            tot_zero += fz;
            tot_other += fot;
            if (last_rb == -2) fail_create = 1;
            else if (last_rb == -3) fail_map = 1;
            else if (last_rb == 1) frames_ok++;
        }

        hr = IDXGISwapChain_Present(sc, 0, 0);
        printf("DXSMOKE: api=d3d11 Present frame=%u hr=0x%08lx (%s)\n",
               frame, (unsigned long)hr, (hr == S_OK) ? "S_OK positive" : (SUCCEEDED(hr) ? "positive status" : "FAILED"));
        if (FAILED(hr))
            fail("IDXGISwapChain::Present", hr);

        hr = ID3D11Device_GetDeviceRemovedReason(dev);
        if (FAILED(hr) || hr != S_OK)
            fail("ID3D11Device_GetDeviceRemovedReason (device removed post-present)", hr);

        /* DISCARD: the RTV may now alias a different image. Rebind each frame. */
        ID3D11RenderTargetView_Release(rtv);
        rtv = NULL;
        bb = NULL;
        hr = IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
        if (FAILED(hr) || !bb)
            fail("IDXGISwapChain::GetBuffer", hr);
        hr = ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv);
        ID3D11Texture2D_Release(bb);
        if (FAILED(hr) || !rtv)
            fail("ID3D11Device::CreateRenderTargetView", hr);
        ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL);
        pump(frame + 1 == nframes ? hold_ms() : 30);
    }
    ID3D11RenderTargetView_Release(rtv);
    if (event) ID3D11Query_Release(event);
    ID3D11DeviceContext_Release(ctx);
    IDXGISwapChain_Release(sc);
    ID3D11Device_Release(dev);

    /* Explicit outcome API vs readback; no unconditional visualPASS */
    const char *rb_desc = "SKIPPED";
    if (staging_on) {
        /* Every frame, every pixel orange, alpha 0xff. One good frame is not PASS. */
        if (fail_create) rb_desc = "STAGING_CREATE_FAIL";
        else if (fail_map) rb_desc = "MAP_FAIL";
        else if (frames_ok == (int)nframes && tot_zero == 0 && tot_other == 0) rb_desc = "ORANGE_PASS";
        else if (tot_orange == 0 && tot_zero > 0 && tot_other == 0) rb_desc = "BLACK_FAIL";
        else if (tot_orange > 0) rb_desc = "ORANGE_PARTIAL";
        else rb_desc = "UNKNOWN";
    }
    printf("DXSMOKE: outcome API=PASS readback=%s (orange=%u zero=%u other=%u) frames=%u\n",
           rb_desc, tot_orange, tot_zero, tot_other, nframes);
    /* API success is not a visual pass. Staging was requested and failed. */
    if (staging_on && strcmp(rb_desc, "ORANGE_PASS") != 0)
        done(4);
    done(0);
}

int main(int argc, char **argv)
{
    const char *api = argc > 1 ? argv[1] : "";
    /* Path-only launcher wrapper: dxsmoke-d3dN.exe selects its API. */
    if (argc == 1) {
        if (strstr(argv[0], "dxsmoke-d3d8")) api = "d3d8";
        else if (strstr(argv[0], "dxsmoke-d3d9")) api = "d3d9";
        else if (strstr(argv[0], "dxsmoke-d3d10")) api = "d3d10";
        else if (strstr(argv[0], "dxsmoke-d3d11")) api = "d3d11";
        if (*api) {
            _putenv("DXSMOKE_HOLD=12000");
        }
    }

    setvbuf(stdout, NULL, _IONBF, 0);
    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);

    if (!strcmp(api, "d3d8"))
        run_d3d8();
    else if (!strcmp(api, "d3d9"))
        run_d3d9();
    else if (!strcmp(api, "d3d10"))
        run_d3d10();
    else if (!strcmp(api, "d3d11"))
        run_d3d11();

    printf("DXSMOKE: FAIL usage: dxsmoke.exe <d3d8|d3d9|d3d10|d3d11>\n");
    done(2);
    return 2;
}
