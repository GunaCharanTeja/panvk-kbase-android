/*
 * Windowed clear/present smoke for one of d3d8, d3d9, d3d10, d3d11.
 * Every graphics entry point is LoadLibrary/GetProcAddress'd, so the
 * binary links against nothing but the CRT (no d3d import libs).
 *
 *   dxsmoke.exe <d3d8|d3d9|d3d10|d3d11>
 *
 * Creates a hardware device on a visible window, clears the back buffer
 * to opaque orange, presents once, logs the HRESULT and adapter string,
 * then exits. A watchdog thread aborts at 10s no matter what.
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
#include <string.h>

/* d3d8types.h and d3d9types.h both define the same enums, so the d3d8
 * path lives in its own translation unit (dxsmoke_d3d8.c). */
void run_d3d8(void);
void done(int code);
void fail(const char *what, HRESULT hr);
void *sym(HMODULE mod, const char *name);
HWND make_window(const char *title);
void pump(DWORD ms);

/* headers only declare these GUIDs; define the four this file takes the
 * address of so nothing has to be pulled out of uuid.lib / dxguid.lib */
const GUID IID_IUnknown = {0x00000000,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
const GUID IID_IDXGIDevice = {0x54ec77fa,0x1377,0x44e6,{0x8c,0x32,0x88,0xfd,0x5f,0x44,0xc8,0x4c}};
const GUID IID_ID3D10Texture2D = {0x9b7e4c04,0x342c,0x4106,{0xa1,0x9f,0x4f,0x27,0x04,0xf6,0x89,0xf0}};
const GUID IID_ID3D11Texture2D = {0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};

#define W 320
#define H 240
#define DEADLINE_MS 10000

static volatile LONG g_done;

static DWORD WINAPI watchdog(void *unused)
{
    (void)unused;
    Sleep(DEADLINE_MS);
    if (!g_done) {
        printf("DXSMOKE: FAIL deadline %ds exceeded\n", DEADLINE_MS / 1000);
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

static void run_d3d9(void)
{
    HMODULE dll = LoadLibraryA("d3d9.dll");
    IDirect3D9 *(WINAPI *create)(UINT);
    IDirect3D9 *d3d;
    IDirect3DDevice9 *dev = NULL;
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
    hr = IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL);
    printf("DXSMOKE: api=d3d9 Present hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr))
        fail("IDirect3DDevice9::Present", hr);

    pump(750);
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
    ID3D10Texture2D_Release(bb);
    if (FAILED(hr) || !rtv)
        fail("ID3D10Device::CreateRenderTargetView", hr);

    ID3D10Device_OMSetRenderTargets(dev, 1, &rtv, NULL);
    ID3D10Device_ClearRenderTargetView(dev, rtv, color);
    printf("DXSMOKE: api=d3d10 Clear hr=0x%08lx\n", 0ul);
    hr = IDXGISwapChain_Present(sc, 0, 0);
    printf("DXSMOKE: api=d3d10 Present hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr))
        fail("IDXGISwapChain::Present", hr);

    pump(750);
    ID3D10RenderTargetView_Release(rtv);
    IDXGISwapChain_Release(sc);
    ID3D10Device_Release(dev);
    printf("DXSMOKE: PASS api=d3d10\n");
    done(0);
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

    hr = IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
    if (FAILED(hr) || !bb)
        fail("IDXGISwapChain::GetBuffer", hr);
    hr = ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv);
    ID3D11Texture2D_Release(bb);
    if (FAILED(hr) || !rtv)
        fail("ID3D11Device::CreateRenderTargetView", hr);

    ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL);
    ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, color);
    printf("DXSMOKE: api=d3d11 Clear hr=0x%08lx\n", 0ul);
    hr = IDXGISwapChain_Present(sc, 0, 0);
    printf("DXSMOKE: api=d3d11 Present hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr))
        fail("IDXGISwapChain::Present", hr);

    pump(750);
    ID3D11RenderTargetView_Release(rtv);
    ID3D11DeviceContext_Release(ctx);
    IDXGISwapChain_Release(sc);
    ID3D11Device_Release(dev);
    printf("DXSMOKE: PASS api=d3d11\n");
    done(0);
}

int main(int argc, char **argv)
{
    const char *api = argc > 1 ? argv[1] : "";

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
