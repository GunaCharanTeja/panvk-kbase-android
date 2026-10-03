/*
 * Draw smoke (NOT a clear smoke): d3d11 or d3d9 windowed, 320x240.
 * Clears to dark blue (NOT orange), then draws
 *   - left : triangle with red/green/blue vertex colors
 *   - right: quad textured with an 8x8 magenta/yellow checker (point sampled)
 * dxdraw.exe <d3d11|d3d9>. Env: DXDRAW_HOLD ms (default 12000),
 * DXDRAW_STAGING=1 also reads the back buffer on the GPU side before Present
 * and prints sample pixels, to split "draws missing" from "presentation".
 * d3d11 compiles HLSL at runtime via d3dcompiler_47.dll. Not a driver test.
 */
#define COBJMACROS
#define CINTERFACE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const GUID IID_ID3D11Texture2D = {0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};
static int W = 320, H = 240; /* DXDRAW_W / DXDRAW_H override */

static int envi(const char *n, int d) { const char *e = getenv(n); return e && *e ? atoi(e) : d; }
static DWORD hold(void) { const char *e = getenv("DXDRAW_HOLD"); return e && *e ? (DWORD)atoi(e) : 12000; }
static void die(const char *w, HRESULT hr)
{
    printf("DXDRAW: FAIL %s hr=0x%08lx\n", w, (unsigned long)hr);
    fflush(stdout);
    ExitProcess(1);
}

static LRESULT CALLBACK wp(HWND h, UINT m, WPARAM w, LPARAM l)
{
    return (m == WM_CLOSE || m == WM_DESTROY) ? 0 : DefWindowProcA(h, m, w, l);
}

static HWND mkwin(const char *t)
{
    WNDCLASSA wc; RECT r = {0, 0, W, H}; HWND h;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wp; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "dxdraw";
    RegisterClassA(&wc);
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW | WS_VISIBLE, FALSE);
    h = CreateWindowA("dxdraw", t, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                      r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    if (!h) die("CreateWindow", E_FAIL);
    ShowWindow(h, SW_SHOW); UpdateWindow(h);
    return h;
}

static void pump(DWORD ms)
{
    MSG m; DWORD t = GetTickCount();
    while (GetTickCount() - t < ms) {
        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
        Sleep(15);
    }
}

/* expected: bg(26,51,153) tri-center ~ mix, quad checker magenta/yellow */
static void sample(const char *tag, const unsigned char *d, unsigned pitch, int bgra)
{
    static const struct { const char *n; int x, y; } pts[] = {
        {"bg", 5, 5}, {"tri_center", 80, 130}, {"quad_a", 215, 90}, {"quad_b", 245, 90}, {"quad_c", 215, 150}, {"quad_d", 245, 150}};
    unsigned bg = 0, other = 0, x, y;
    for (unsigned i = 0; i < sizeof(pts) / sizeof(pts[0]); i++) {
        const unsigned char *p = d + (size_t)pts[i].y * pitch + pts[i].x * 4;
        printf("DXDRAW: %s %s(%d,%d)=%u,%u,%u\n", tag, pts[i].n, pts[i].x, pts[i].y,
               bgra ? p[2] : p[0], p[1], bgra ? p[0] : p[2]);
    }
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            const unsigned char *p = d + (size_t)y * pitch + x * 4;
            unsigned r = bgra ? p[2] : p[0], g = p[1], b = bgra ? p[0] : p[2];
            if (abs((int)r - 26) < 4 && abs((int)g - 51) < 4 && abs((int)b - 153) < 4) bg++; else other++;
        }
    printf("DXDRAW: %s histogram bg=%u non_bg=%u (non_bg==0 => draws missing)\n", tag, bg, other);
}

/* ----------------------------------------------------------------- d3d11 */
static const char *HLSL =
    "struct VI{float2 p:POSITION;float4 c:COLOR;float2 uv:TEXCOORD;};\n"
    "struct VO{float4 p:SV_Position;float4 c:COLOR;float2 uv:TEXCOORD;};\n"
    "VO vs(VI i){VO o;o.p=float4(i.p,0,1);o.c=i.c;o.uv=i.uv;return o;}\n"
    "Texture2D t:register(t0);SamplerState s:register(s0);\n"
    "float4 psc(VO i):SV_Target{return i.c;}\n"
    "float4 pst(VO i):SV_Target{return t.Sample(s,i.uv);}\n";

typedef struct { float x, y, r, g, b, a, u, v; } Vtx;

static ID3DBlob *comp(HRESULT (WINAPI *C)(const void *, SIZE_T, const char *, const D3D_SHADER_MACRO *,
                      ID3DInclude *, const char *, const char *, UINT, UINT, ID3DBlob **, ID3DBlob **),
                      const char *entry, const char *prof)
{
    ID3DBlob *b = NULL, *e = NULL;
    HRESULT hr = C(HLSL, strlen(HLSL), "x", NULL, NULL, entry, prof, 0, 0, &b, &e);
    if (FAILED(hr)) {
        printf("DXDRAW: D3DCompile %s: %s\n", entry, e ? (char *)ID3D10Blob_GetBufferPointer(e) : "?");
        die("D3DCompile", hr);
    }
    return b;
}

static void run11(void)
{
    HMODULE dll = LoadLibraryA("d3d11.dll"), cdll = LoadLibraryA("d3dcompiler_47.dll");
    HRESULT (WINAPI *create)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT,
        const DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
    IDXGISwapChain *sc; ID3D11Device *dev; ID3D11DeviceContext *ctx; D3D_FEATURE_LEVEL fl = 0;
    DXGI_SWAP_CHAIN_DESC sd; HRESULT hr; HWND hw;
    ID3D11VertexShader *vs; ID3D11PixelShader *psc, *pst; ID3D11InputLayout *il;
    ID3D11Buffer *vbt, *vbq; ID3D11Texture2D *tex, *bb; ID3D11ShaderResourceView *srv;
    ID3D11SamplerState *smp; ID3D11RenderTargetView *rtv; ID3DBlob *bvs, *bpc, *bpt;
    D3D11_INPUT_ELEMENT_DESC ied[3] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    Vtx tri[3] = {{-0.9f,-0.8f,1,0,0,1,0,0},{-0.5f,0.8f,0,0,1,1,0,0},{-0.1f,-0.8f,0,1,0,1,0,0}}; /* CW = front */
    Vtx quad[4] = {{0.1f,0.8f,1,1,1,1,0,0},{0.9f,0.8f,1,1,1,1,1,0},{0.1f,-0.8f,1,1,1,1,0,1},{0.9f,-0.8f,1,1,1,1,1,1}};
    unsigned texd[64]; int i, f; UINT stride = sizeof(Vtx), off = 0;
    const float bg[4] = {0.1f, 0.2f, 0.6f, 1.0f};
    D3D11_BUFFER_DESC bd; D3D11_SUBRESOURCE_DATA sr; D3D11_TEXTURE2D_DESC td; D3D11_SAMPLER_DESC smd;
    D3D11_VIEWPORT vp = {0, 0, W, H, 0, 1};
    ID3D11Texture2D *dtex = NULL, *mtex = NULL; ID3D11DepthStencilView *dsv = NULL; ID3D11RenderTargetView *mrtv = NULL;
    int msaa = envi("DXDRAW_MSAA", 0), depth = envi("DXDRAW_DEPTH", 0), nframes = envi("DXDRAW_FRAMES", 8);
    HRESULT (WINAPI *C)(const void *, SIZE_T, const char *, const D3D_SHADER_MACRO *, ID3DInclude *,
                        const char *, const char *, UINT, UINT, ID3DBlob **, ID3DBlob **);

    if (!dll || !cdll) die("LoadLibrary d3d11/d3dcompiler_47", E_FAIL);
    create = (void *)GetProcAddress(dll, "D3D11CreateDeviceAndSwapChain");
    C = (void *)GetProcAddress(cdll, "D3DCompile");
    hw = mkwin("dxdraw d3d11");
    memset(&sd, 0, sizeof(sd));
    sd.BufferDesc.Width = W; sd.BufferDesc.Height = H; sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = envi("DXDRAW_BUFS", 1);
    sd.OutputWindow = hw; sd.Windowed = TRUE;
    /* DXDRAW_FLIP: 3=FLIP_SEQUENTIAL 4=FLIP_DISCARD (needs >=2 buffers) */
    sd.SwapEffect = envi("DXDRAW_FLIP", 0) ? (DXGI_SWAP_EFFECT)envi("DXDRAW_FLIP", 0) : DXGI_SWAP_EFFECT_DISCARD;
    if (envi("DXDRAW_FLIP", 0) && sd.BufferCount < 2) sd.BufferCount = 2;
    hr = create(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx);
    printf("DXDRAW: d3d11 create hr=0x%08lx fl=0x%x\n", (unsigned long)hr, (unsigned)fl);
    if (FAILED(hr)) die("create", hr);

    bvs = comp(C, "vs", "vs_4_0"); bpc = comp(C, "psc", "ps_4_0"); bpt = comp(C, "pst", "ps_4_0");
    hr = ID3D11Device_CreateVertexShader(dev, ID3D10Blob_GetBufferPointer(bvs), ID3D10Blob_GetBufferSize(bvs), NULL, &vs);
    if (FAILED(hr)) die("CreateVS", hr);
    hr = ID3D11Device_CreatePixelShader(dev, ID3D10Blob_GetBufferPointer(bpc), ID3D10Blob_GetBufferSize(bpc), NULL, &psc);
    if (FAILED(hr)) die("CreatePS c", hr);
    hr = ID3D11Device_CreatePixelShader(dev, ID3D10Blob_GetBufferPointer(bpt), ID3D10Blob_GetBufferSize(bpt), NULL, &pst);
    if (FAILED(hr)) die("CreatePS t", hr);
    hr = ID3D11Device_CreateInputLayout(dev, ied, 3, ID3D10Blob_GetBufferPointer(bvs), ID3D10Blob_GetBufferSize(bvs), &il);
    if (FAILED(hr)) die("CreateInputLayout", hr);

    memset(&bd, 0, sizeof(bd)); bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; bd.Usage = D3D11_USAGE_IMMUTABLE;
    memset(&sr, 0, sizeof(sr));
    bd.ByteWidth = sizeof(tri); sr.pSysMem = tri;
    if (FAILED(hr = ID3D11Device_CreateBuffer(dev, &bd, &sr, &vbt))) die("VB tri", hr);
    bd.ByteWidth = sizeof(quad); sr.pSysMem = quad;
    if (FAILED(hr = ID3D11Device_CreateBuffer(dev, &bd, &sr, &vbq))) die("VB quad", hr);

    for (i = 0; i < 64; i++) texd[i] = (((i & 7) ^ (i >> 3)) & 1) ? 0xff00ffffu /*ABGR yellow*/ : 0xffff00ffu /*magenta*/;
    memset(&td, 0, sizeof(td)); td.Width = td.Height = 8; td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    sr.pSysMem = texd; sr.SysMemPitch = 32;
    if (FAILED(hr = ID3D11Device_CreateTexture2D(dev, &td, &sr, &tex))) die("CreateTexture2D", hr);
    if (FAILED(hr = ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)tex, NULL, &srv))) die("SRV", hr);
    memset(&smd, 0, sizeof(smd)); smd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    smd.AddressU = smd.AddressV = smd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; smd.MaxLOD = D3D11_FLOAT32_MAX;
    smd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    if (FAILED(hr = ID3D11Device_CreateSamplerState(dev, &smd, &smp))) die("Sampler", hr);

    if (depth) {
        D3D11_TEXTURE2D_DESC dd; memset(&dd, 0, sizeof(dd));
        dd.Width = W; dd.Height = H; dd.MipLevels = dd.ArraySize = 1; dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dd.SampleDesc.Count = msaa ? msaa : 1; dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        if (FAILED(hr = ID3D11Device_CreateTexture2D(dev, &dd, NULL, &dtex))) die("depth tex", hr);
        if (FAILED(hr = ID3D11Device_CreateDepthStencilView(dev, (ID3D11Resource *)dtex, NULL, &dsv))) die("DSV", hr);
    }
    if (msaa) {
        D3D11_TEXTURE2D_DESC md; memset(&md, 0, sizeof(md));
        md.Width = W; md.Height = H; md.MipLevels = md.ArraySize = 1; md.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        md.SampleDesc.Count = msaa; md.BindFlags = D3D11_BIND_RENDER_TARGET;
        if (FAILED(hr = ID3D11Device_CreateTexture2D(dev, &md, NULL, &mtex))) die("msaa tex", hr);
        if (FAILED(hr = ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)mtex, NULL, &mrtv))) die("msaa RTV", hr);
    }
    for (f = 0; f < nframes; f++) {
        bb = NULL; rtv = NULL;
        if (FAILED(hr = IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb))) die("GetBuffer", hr);
        if (FAILED(hr = ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv))) die("RTV", hr);
        { ID3D11RenderTargetView *t = msaa ? mrtv : rtv;
          ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &t, dsv);
          ID3D11DeviceContext_RSSetViewports(ctx, 1, &vp);
          ID3D11DeviceContext_ClearRenderTargetView(ctx, t, bg);
          if (dsv) ID3D11DeviceContext_ClearDepthStencilView(ctx, dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0); }
        ID3D11DeviceContext_IASetInputLayout(ctx, il);
        ID3D11DeviceContext_VSSetShader(ctx, vs, NULL, 0);
        ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vbt, &stride, &off);
        ID3D11DeviceContext_PSSetShader(ctx, psc, NULL, 0);
        ID3D11DeviceContext_Draw(ctx, 3, 0);
        ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vbq, &stride, &off);
        ID3D11DeviceContext_PSSetShader(ctx, pst, NULL, 0);
        ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &srv);
        ID3D11DeviceContext_PSSetSamplers(ctx, 0, 1, &smp);
        ID3D11DeviceContext_Draw(ctx, 4, 0);
        if (msaa) ID3D11DeviceContext_ResolveSubresource(ctx, (ID3D11Resource *)bb, 0, (ID3D11Resource *)mtex, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
        if (getenv("DXDRAW_STAGING") && f == nframes - 1) {
            D3D11_TEXTURE2D_DESC sdsc; ID3D11Texture2D *st; D3D11_MAPPED_SUBRESOURCE mp;
            ID3D11Texture2D_GetDesc(bb, &sdsc);
            sdsc.Usage = D3D11_USAGE_STAGING; sdsc.BindFlags = 0; sdsc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; sdsc.MiscFlags = 0;
            if (SUCCEEDED(ID3D11Device_CreateTexture2D(dev, &sdsc, NULL, &st))) {
                ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)st, (ID3D11Resource *)bb);
                if (SUCCEEDED(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &mp))) {
                    sample("gpu-readback", mp.pData, mp.RowPitch, 1);
                    ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)st, 0);
                } else printf("DXDRAW: staging Map failed\n");
                ID3D11Texture2D_Release(st);
            } else printf("DXDRAW: staging create failed\n");
        }
        ID3D11Texture2D_Release(bb);
        ID3D11RenderTargetView_Release(rtv);
        hr = IDXGISwapChain_Present(sc, 0, 0);
        printf("DXDRAW: d3d11 Present frame=%d hr=0x%08lx\n", f, (unsigned long)hr);
        if (FAILED(hr)) die("Present", hr);
        pump(f == nframes - 1 ? hold() : envi("DXDRAW_PUMP", 30));
    }
    printf("DXDRAW: PASS api=d3d11 (API only; check pixels)\n");
    fflush(stdout);
    ExitProcess(0);
}

/* ------------------------------------------------------------------ d3d9 */
typedef struct { float x, y, z, rhw; DWORD c; float u, v; } V9;

static void run9(void)
{
    HMODULE dll = LoadLibraryA("d3d9.dll");
    IDirect3D9 *(WINAPI *create)(UINT);
    IDirect3D9 *d3d; IDirect3DDevice9 *dev = NULL; IDirect3DTexture9 *tex; D3DLOCKED_RECT lr;
    D3DPRESENT_PARAMETERS pp; HRESULT hr; HWND hw; int f, i, x, y;
    V9 tri[3] = {{20,220,0,1,0xffff0000,0,0},{150,220,0,1,0xff00ff00,0,0},{85,20,0,1,0xff0000ff,0,0}};
    V9 quad[4] = {{170,20,0,1,0xffffffff,0,0},{300,20,0,1,0xffffffff,1,0},{170,220,0,1,0xffffffff,0,1},{300,220,0,1,0xffffffff,1,1}};

    if (!dll) die("LoadLibrary d3d9", E_FAIL);
    create = (void *)GetProcAddress(dll, "Direct3DCreate9");
    d3d = create(D3D_SDK_VERSION);
    hw = mkwin("dxdraw d3d9");
    memset(&pp, 0, sizeof(pp));
    pp.BackBufferWidth = W; pp.BackBufferHeight = H; pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow = hw; pp.Windowed = TRUE;
    hr = IDirect3D9_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
    printf("DXDRAW: d3d9 CreateDevice hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr)) die("CreateDevice", hr);
    hr = IDirect3DDevice9_CreateTexture(dev, 8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL);
    if (FAILED(hr)) die("CreateTexture", hr);
    IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, 0);
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++)
        ((DWORD *)((char *)lr.pBits + y * lr.Pitch))[x] = ((x ^ y) & 1) ? 0xffffff00 : 0xffff00ff;
    IDirect3DTexture9_UnlockRect(tex, 0);
    for (i = 0; i < 3; i++) { tri[i].x *= W / 320.0f; tri[i].y *= H / 240.0f; }
    for (i = 0; i < 4; i++) { quad[i].x *= W / 320.0f; quad[i].y *= H / 240.0f; }
    for (f = 0; f < envi("DXDRAW_FRAMES", 8); f++) {
        IDirect3DDevice9_Clear(dev, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(26, 51, 153), 1.0f, 0);
        IDirect3DDevice9_BeginScene(dev);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
        IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        IDirect3DDevice9_SetTexture(dev, 0, NULL);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLELIST, 1, tri, sizeof(V9));
        IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9 *)tex);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(V9));
        IDirect3DDevice9_EndScene(dev);
        hr = IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL);
        printf("DXDRAW: d3d9 Present frame=%d hr=0x%08lx\n", f, (unsigned long)hr);
        pump(f == envi("DXDRAW_FRAMES", 8) - 1 ? hold() : 30);
    }
    (void)i;
    printf("DXDRAW: PASS api=d3d9 (API only; check pixels)\n");
    fflush(stdout);
    ExitProcess(0);
}

int main(int argc, char **argv)
{
    const char *api = argc > 1 ? argv[1] : (strstr(argv[0], "d3d9") ? "d3d9" : "d3d11");
    setvbuf(stdout, NULL, _IONBF, 0);
    W = envi("DXDRAW_W", W); H = envi("DXDRAW_H", H);
    if (!strcmp(api, "d3d11")) run11();
    else if (!strcmp(api, "d3d9")) run9();
    printf("DXDRAW: usage dxdraw.exe <d3d11|d3d9>\n");
    return 2;
}
