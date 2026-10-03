/*
 * "Test Direct3D" demo: 2x2 rotating lit rounded blocks (green/red/yellow/blue)
 * on a grey background, indexed vertex+index buffers with normals, depth buffer.
 * dxcube.exe <d3d11|d3d10|d3d9|d3d8>. Env: DXCUBE_SECS (default 25) render time, keeps
 * presenting the whole time so screenshots hit a live frame.
 * d3d9 = fixed function lighting; d3d11 = HLSL via d3dcompiler_47.dll.
 */
#define COBJMACROS
#define CINTERFACE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* d3d8 and d3d9 headers clash: build with -DDXCUBE_D3D8 for the d3d8 binary. */
#ifdef DXCUBE_D3D8
#include <d3d8.h>
#else
#include <d3d9.h>
#include <d3d11.h>
#include <d3d10.h>
#include <d3dcompiler.h>
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int W = 1280;
static int H = 720;
#define N 8                           /* grid cells per face edge */
#define NV (6 * (N + 1) * (N + 1))
#define NI (6 * N * N * 6)

typedef struct { float p[3], n[3]; } Vtx;
static Vtx vtx[NV];
static unsigned short idx[NI];
static const float blk[4][2] = {{-1.3f, 1.3f}, {1.3f, 1.3f}, {-1.3f, -1.3f}, {1.3f, -1.3f}};
static const float col[4][4] = {{0.1f, 0.8f, 0.1f, 1}, {0.9f, 0.1f, 0.1f, 1}, {0.95f, 0.85f, 0.1f, 1}, {0.15f, 0.25f, 0.95f, 1}};
static const float bg[4] = {0.5f, 0.5f, 0.5f, 1.0f};

static int envi(const char *n, int d) { const char *e = getenv(n); return e && *e ? atoi(e) : d; }
static void die(const char *w, HRESULT hr)
{
    printf("CUBE: FAIL %s hr=0x%08lx\n", w, (unsigned long)hr);
    fflush(stdout);
    ExitProcess(1);
}

static LRESULT CALLBACK wp(HWND h, UINT m, WPARAM w, LPARAM l)
{
    return (m == WM_CLOSE || m == WM_DESTROY) ? 0 : DefWindowProcA(h, m, w, l);
}

static HWND mkwin(void)
{
    WNDCLASSA wc; RECT r = {0, 0, W, H}; HWND h;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wp; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "dxcube";
    RegisterClassA(&wc);
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW | WS_VISIBLE, FALSE);
    h = CreateWindowA("dxcube", "Test Direct3D", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                      r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    if (!h) die("CreateWindow", E_FAIL);
    ShowWindow(h, SW_SHOW); UpdateWindow(h);
    return h;
}

static void pump(DWORD ms)
{
    MSG m; DWORD t = GetTickCount();
    do {
        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
        Sleep(5);
    } while (GetTickCount() - t < ms);
}

/* ---- rounded box mesh: cube grid pushed onto a rounded surface ---- */
static void mkmesh(void)
{
    const float r = 0.35f, in = 1.0f - r;
    int a, s, i, j, nv = 0, ni = 0;
    for (a = 0; a < 3; a++)
        for (s = -1; s <= 1; s += 2) {
            int base = nv, u = (a + 1) % 3, v = (a + 2) % 3;
            for (j = 0; j <= N; j++)
                for (i = 0; i <= N; i++) {
                    float p[3], q[3], d[3], len; int k;
                    p[a] = (float)s; p[u] = -1 + 2.0f * i / N; p[v] = -1 + 2.0f * j / N;
                    for (k = 0; k < 3; k++) { q[k] = p[k] > in ? in : (p[k] < -in ? -in : p[k]); d[k] = p[k] - q[k]; }
                    len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                    for (k = 0; k < 3; k++) {
                        vtx[nv].n[k] = d[k] / len;
                        vtx[nv].p[k] = q[k] + vtx[nv].n[k] * r;
                    }
                    nv++;
                }
            for (j = 0; j < N; j++)
                for (i = 0; i < N; i++) {
                    unsigned short a0 = base + j * (N + 1) + i, a1 = a0 + 1, a2 = a0 + N + 1, a3 = a2 + 1;
                    idx[ni++] = a0; idx[ni++] = a2; idx[ni++] = a1;
                    idx[ni++] = a1; idx[ni++] = a2; idx[ni++] = a3;
                }
        }
}

/* ---- row-major, row-vector (D3D) matrices ---- */
typedef struct { float m[16]; } M4;
static M4 mmul(M4 a, M4 b)
{
    M4 c; int i, j, k;
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) {
        float s = 0; for (k = 0; k < 4; k++) s += a.m[i * 4 + k] * b.m[k * 4 + j];
        c.m[i * 4 + j] = s;
    }
    return c;
}
static M4 ident(void) { M4 m; memset(&m, 0, sizeof(m)); m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1; return m; }
static M4 rotx(float a) { M4 m = ident(); m.m[5] = cosf(a); m.m[6] = sinf(a); m.m[9] = -sinf(a); m.m[10] = cosf(a); return m; }
static M4 roty(float a) { M4 m = ident(); m.m[0] = cosf(a); m.m[2] = -sinf(a); m.m[8] = sinf(a); m.m[10] = cosf(a); return m; }
static M4 trans(float x, float y, float z) { M4 m = ident(); m.m[12] = x; m.m[13] = y; m.m[14] = z; return m; }
static M4 proj(void)
{
    float ys = 1.0f / tanf(0.5f), xs = ys * (float)H / W, zn = 1, zf = 50;
    M4 m; memset(&m, 0, sizeof(m));
    m.m[0] = xs; m.m[5] = ys; m.m[10] = zf / (zf - zn); m.m[11] = 1; m.m[14] = -zn * zf / (zf - zn);
    return m;
}
static M4 world(int b, float t)
{
    return mmul(mmul(rotx(t * (0.7f + 0.15f * b)), roty(t * (1.0f + 0.1f * b))), trans(blk[b][0], blk[b][1], 0));
}

#ifndef DXCUBE_D3D8
/* ----------------------------------------------------------------- d3d11 */
static const char *HLSL =
    "cbuffer cb:register(b0){row_major float4x4 wvp;row_major float4x4 w;float4 col;};\n"
    "struct VI{float3 p:POSITION;float3 n:NORMAL;};\n"
    "struct VO{float4 p:SV_Position;float3 n:TEXCOORD0;};\n"
    "VO vs(VI i){VO o;o.p=mul(float4(i.p,1),wvp);o.n=mul(i.n,(float3x3)w);return o;}\n"
    "float4 ps(VO i):SV_Target{float3 n=normalize(i.n);float3 l=normalize(float3(-0.3,0.5,-0.8));\n"
    "float d=saturate(dot(n,l));float3 h=normalize(l+float3(0,0,-1));float s=pow(saturate(dot(n,h)),32)*0.4;\n"
    "return float4(col.rgb*(0.25+0.75*d)+s,1);}\n";

static ID3DBlob *comp(HRESULT (WINAPI *C)(const void *, SIZE_T, const char *, const D3D_SHADER_MACRO *,
                      ID3DInclude *, const char *, const char *, UINT, UINT, ID3DBlob **, ID3DBlob **),
                      const char *entry, const char *prof)
{
    ID3DBlob *b = NULL, *e = NULL;
    HRESULT hr = C(HLSL, strlen(HLSL), "x", NULL, NULL, entry, prof, 0, 0, &b, &e);
    if (FAILED(hr)) {
        printf("CUBE: D3DCompile %s: %s\n", entry, e ? (char *)ID3D10Blob_GetBufferPointer(e) : "?");
        die("D3DCompile", hr);
    }
    return b;
}

static const GUID IID_Tex2D = {0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};

static void run11(int secs)
{
    HMODULE dll = LoadLibraryA("d3d11.dll"), cdll = LoadLibraryA("d3dcompiler_47.dll");
    HRESULT (WINAPI *create)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT,
        const DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
    HRESULT (WINAPI *C)(const void *, SIZE_T, const char *, const D3D_SHADER_MACRO *, ID3DInclude *,
                        const char *, const char *, UINT, UINT, ID3DBlob **, ID3DBlob **);
    IDXGISwapChain *sc; ID3D11Device *dev; ID3D11DeviceContext *ctx; D3D_FEATURE_LEVEL fl = 0;
    DXGI_SWAP_CHAIN_DESC sd; HRESULT hr; HWND hw; DWORD t0; int f = 0, b;
    ID3D11VertexShader *vs; ID3D11PixelShader *ps; ID3D11InputLayout *il;
    ID3D11Buffer *vb, *ib, *cb; ID3D11Texture2D *bb, *dtex; ID3D11RenderTargetView *rtv; ID3D11DepthStencilView *dsv;
    ID3D11RasterizerState *rs; ID3DBlob *bvs, *bps;
    D3D11_INPUT_ELEMENT_DESC ied[2] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    D3D11_BUFFER_DESC bd; D3D11_SUBRESOURCE_DATA sr; D3D11_TEXTURE2D_DESC dd; D3D11_RASTERIZER_DESC rd;
    D3D11_VIEWPORT vp = {0, 0, W, H, 0, 1};
    UINT stride = sizeof(Vtx), off = 0;
    M4 vp_ = mmul(trans(0, 0, 8), proj());
    struct { M4 wvp, w; float c[4]; } cbd;

    if (!dll || !cdll) die("LoadLibrary d3d11/d3dcompiler_47", E_FAIL);
    create = (void *)GetProcAddress(dll, "D3D11CreateDeviceAndSwapChain");
    C = (void *)GetProcAddress(cdll, "D3DCompile");
    hw = mkwin();
    memset(&sd, 0, sizeof(sd));
    sd.BufferDesc.Width = W; sd.BufferDesc.Height = H; sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = 1;
    sd.OutputWindow = hw; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    hr = create(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx);
    printf("CUBE: d3d11 create hr=0x%08lx fl=0x%x\n", (unsigned long)hr, (unsigned)fl);
    if (FAILED(hr)) die("create", hr);

    bvs = comp(C, "vs", "vs_4_0"); bps = comp(C, "ps", "ps_4_0");
    if (FAILED(hr = ID3D11Device_CreateVertexShader(dev, ID3D10Blob_GetBufferPointer(bvs), ID3D10Blob_GetBufferSize(bvs), NULL, &vs))) die("VS", hr);
    if (FAILED(hr = ID3D11Device_CreatePixelShader(dev, ID3D10Blob_GetBufferPointer(bps), ID3D10Blob_GetBufferSize(bps), NULL, &ps))) die("PS", hr);
    if (FAILED(hr = ID3D11Device_CreateInputLayout(dev, ied, 2, ID3D10Blob_GetBufferPointer(bvs), ID3D10Blob_GetBufferSize(bvs), &il))) die("IL", hr);

    memset(&bd, 0, sizeof(bd)); memset(&sr, 0, sizeof(sr));
    bd.Usage = D3D11_USAGE_IMMUTABLE; bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; bd.ByteWidth = sizeof(vtx); sr.pSysMem = vtx;
    if (FAILED(hr = ID3D11Device_CreateBuffer(dev, &bd, &sr, &vb))) die("VB", hr);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER; bd.ByteWidth = sizeof(idx); sr.pSysMem = idx;
    if (FAILED(hr = ID3D11Device_CreateBuffer(dev, &bd, &sr, &ib))) die("IB", hr);
    bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; bd.ByteWidth = sizeof(cbd);
    if (FAILED(hr = ID3D11Device_CreateBuffer(dev, &bd, NULL, &cb))) die("CB", hr);

    memset(&dd, 0, sizeof(dd));
    dd.Width = W; dd.Height = H; dd.MipLevels = dd.ArraySize = 1; dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1; dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(hr = ID3D11Device_CreateTexture2D(dev, &dd, NULL, &dtex))) die("depth tex", hr);
    if (FAILED(hr = ID3D11Device_CreateDepthStencilView(dev, (ID3D11Resource *)dtex, NULL, &dsv))) die("DSV", hr);
    memset(&rd, 0, sizeof(rd)); rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    if (FAILED(hr = ID3D11Device_CreateRasterizerState(dev, &rd, &rs))) die("RS", hr);

    t0 = GetTickCount();
    while (GetTickCount() - t0 < (DWORD)secs * 1000) {
        float t = f * 0.04f;
        if (FAILED(hr = IDXGISwapChain_GetBuffer(sc, 0, &IID_Tex2D, (void **)&bb))) die("GetBuffer", hr);
        if (FAILED(hr = ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv))) die("RTV", hr);
        ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, dsv);
        ID3D11DeviceContext_RSSetViewports(ctx, 1, &vp);
        ID3D11DeviceContext_RSSetState(ctx, rs);
        ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, bg);
        ID3D11DeviceContext_ClearDepthStencilView(ctx, dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        ID3D11DeviceContext_IASetInputLayout(ctx, il);
        ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vb, &stride, &off);
        ID3D11DeviceContext_IASetIndexBuffer(ctx, ib, DXGI_FORMAT_R16_UINT, 0);
        ID3D11DeviceContext_VSSetShader(ctx, vs, NULL, 0);
        ID3D11DeviceContext_PSSetShader(ctx, ps, NULL, 0);
        ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, &cb);
        ID3D11DeviceContext_PSSetConstantBuffers(ctx, 0, 1, &cb);
        for (b = 0; b < 4; b++) {
            cbd.w = world(b, t); cbd.wvp = mmul(cbd.w, vp_); memcpy(cbd.c, col[b], 16);
            ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource *)cb, 0, NULL, &cbd, 0, 0);
            ID3D11DeviceContext_DrawIndexed(ctx, NI, 0, 0);
        }
        ID3D11Texture2D_Release(bb);
        ID3D11RenderTargetView_Release(rtv);
        hr = IDXGISwapChain_Present(sc, 0, 0);
        if (FAILED(hr)) die("Present", hr);
        if (f < 10 || f % 30 == 0) printf("CUBE: d3d11 Present frame=%d hr=0x%08lx\n", f, (unsigned long)hr);
        f++;
        pump(10);
    }
    printf("CUBE: PASS api=d3d11 frames=%d (check pixels)\n", f);
    fflush(stdout);
    ExitProcess(0);
}

/* ----------------------------------------------------------------- d3d10 */
static void run10(int secs)
{
    HMODULE dll = LoadLibraryA("d3d10.dll"), cdll = LoadLibraryA("d3dcompiler_47.dll");
    HRESULT (WINAPI *create)(IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, DXGI_SWAP_CHAIN_DESC *,
                             IDXGISwapChain **, ID3D10Device **);
    HRESULT (WINAPI *C)(const void *, SIZE_T, const char *, const D3D_SHADER_MACRO *, ID3DInclude *,
                        const char *, const char *, UINT, UINT, ID3DBlob **, ID3DBlob **);
    IDXGISwapChain *sc; ID3D10Device *dev;
    DXGI_SWAP_CHAIN_DESC sd; HRESULT hr; HWND hw; DWORD t0; int f = 0, b;
    ID3D10VertexShader *vs; ID3D10PixelShader *ps; ID3D10InputLayout *il;
    ID3D10Buffer *vb, *ib, *cb; ID3D10Texture2D *bb, *dtex; ID3D10RenderTargetView *rtv; ID3D10DepthStencilView *dsv;
    ID3D10RasterizerState *rs; ID3DBlob *bvs, *bps;
    D3D10_INPUT_ELEMENT_DESC ied[2] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0}};
    D3D10_BUFFER_DESC bd; D3D10_SUBRESOURCE_DATA sr; D3D10_TEXTURE2D_DESC dd; D3D10_RASTERIZER_DESC rd;
    D3D10_VIEWPORT vp = {0, 0, W, H, 0, 1};
    UINT stride = sizeof(Vtx), off = 0;
    M4 vp_ = mmul(trans(0, 0, 8), proj());
    struct { M4 wvp, w; float c[4]; } cbd;

    if (!dll || !cdll) die("LoadLibrary d3d10/d3dcompiler_47", E_FAIL);
    create = (void *)GetProcAddress(dll, "D3D10CreateDeviceAndSwapChain");
    C = (void *)GetProcAddress(cdll, "D3DCompile");
    hw = mkwin();
    memset(&sd, 0, sizeof(sd));
    sd.BufferDesc.Width = W; sd.BufferDesc.Height = H; sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = 1;
    sd.OutputWindow = hw; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    hr = create(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_SDK_VERSION, &sd, &sc, &dev);
    printf("CUBE: d3d10 create hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr)) die("create", hr);

    bvs = comp(C, "vs", "vs_4_0"); bps = comp(C, "ps", "ps_4_0");
    if (FAILED(hr = ID3D10Device_CreateVertexShader(dev, ID3D10Blob_GetBufferPointer(bvs), ID3D10Blob_GetBufferSize(bvs), &vs))) die("VS", hr);
    if (FAILED(hr = ID3D10Device_CreatePixelShader(dev, ID3D10Blob_GetBufferPointer(bps), ID3D10Blob_GetBufferSize(bps), &ps))) die("PS", hr);
    if (FAILED(hr = ID3D10Device_CreateInputLayout(dev, ied, 2, ID3D10Blob_GetBufferPointer(bvs), ID3D10Blob_GetBufferSize(bvs), &il))) die("IL", hr);

    memset(&bd, 0, sizeof(bd)); memset(&sr, 0, sizeof(sr));
    bd.Usage = D3D10_USAGE_IMMUTABLE; bd.BindFlags = D3D10_BIND_VERTEX_BUFFER; bd.ByteWidth = sizeof(vtx); sr.pSysMem = vtx;
    if (FAILED(hr = ID3D10Device_CreateBuffer(dev, &bd, &sr, &vb))) die("VB", hr);
    bd.BindFlags = D3D10_BIND_INDEX_BUFFER; bd.ByteWidth = sizeof(idx); sr.pSysMem = idx;
    if (FAILED(hr = ID3D10Device_CreateBuffer(dev, &bd, &sr, &ib))) die("IB", hr);
    bd.Usage = D3D10_USAGE_DEFAULT; bd.BindFlags = D3D10_BIND_CONSTANT_BUFFER; bd.ByteWidth = sizeof(cbd);
    if (FAILED(hr = ID3D10Device_CreateBuffer(dev, &bd, NULL, &cb))) die("CB", hr);

    memset(&dd, 0, sizeof(dd));
    dd.Width = W; dd.Height = H; dd.MipLevels = dd.ArraySize = 1; dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1; dd.BindFlags = D3D10_BIND_DEPTH_STENCIL;
    if (FAILED(hr = ID3D10Device_CreateTexture2D(dev, &dd, NULL, &dtex))) die("depth tex", hr);
    if (FAILED(hr = ID3D10Device_CreateDepthStencilView(dev, (ID3D10Resource *)dtex, NULL, &dsv))) die("DSV", hr);
    memset(&rd, 0, sizeof(rd)); rd.FillMode = D3D10_FILL_SOLID; rd.CullMode = D3D10_CULL_NONE; rd.DepthClipEnable = TRUE;
    if (FAILED(hr = ID3D10Device_CreateRasterizerState(dev, &rd, &rs))) die("RS", hr);

    t0 = GetTickCount();
    while (GetTickCount() - t0 < (DWORD)secs * 1000) {
        float t = f * 0.04f;
        if (FAILED(hr = IDXGISwapChain_GetBuffer(sc, 0, &IID_Tex2D, (void **)&bb))) die("GetBuffer", hr);
        if (FAILED(hr = ID3D10Device_CreateRenderTargetView(dev, (ID3D10Resource *)bb, NULL, &rtv))) die("RTV", hr);
        ID3D10Device_OMSetRenderTargets(dev, 1, &rtv, dsv);
        ID3D10Device_RSSetViewports(dev, 1, &vp);
        ID3D10Device_RSSetState(dev, rs);
        ID3D10Device_ClearRenderTargetView(dev, rtv, bg);
        ID3D10Device_ClearDepthStencilView(dev, dsv, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.0f, 0);
        ID3D10Device_IASetInputLayout(dev, il);
        ID3D10Device_IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D10Device_IASetVertexBuffers(dev, 0, 1, &vb, &stride, &off);
        ID3D10Device_IASetIndexBuffer(dev, ib, DXGI_FORMAT_R16_UINT, 0);
        ID3D10Device_VSSetShader(dev, vs);
        ID3D10Device_PSSetShader(dev, ps);
        ID3D10Device_VSSetConstantBuffers(dev, 0, 1, &cb);
        ID3D10Device_PSSetConstantBuffers(dev, 0, 1, &cb);
        for (b = 0; b < 4; b++) {
            cbd.w = world(b, t); cbd.wvp = mmul(cbd.w, vp_); memcpy(cbd.c, col[b], 16);
            ID3D10Device_UpdateSubresource(dev, (ID3D10Resource *)cb, 0, NULL, &cbd, 0, 0);
            ID3D10Device_DrawIndexed(dev, NI, 0, 0);
        }
        ID3D10Texture2D_Release(bb);
        ID3D10RenderTargetView_Release(rtv);
        hr = IDXGISwapChain_Present(sc, 0, 0);
        if (FAILED(hr)) die("Present", hr);
        if (f < 10 || f % 30 == 0) printf("CUBE: d3d10 Present frame=%d hr=0x%08lx\n", f, (unsigned long)hr);
        f++;
        pump(10);
    }
    printf("CUBE: PASS api=d3d10 frames=%d (check pixels)\n", f);
    fflush(stdout);
    ExitProcess(0);
}

/* ------------------------------------------------------------------ d3d9 */
static void run9(int secs)
{
    HMODULE dll = LoadLibraryA("d3d9.dll");
    IDirect3D9 *(WINAPI *create)(UINT);
    IDirect3D9 *d3d; IDirect3DDevice9 *dev = NULL; IDirect3DVertexBuffer9 *vb; IDirect3DIndexBuffer9 *ib;
    D3DPRESENT_PARAMETERS pp; D3DLIGHT9 lt; D3DMATERIAL9 mt; HRESULT hr; HWND hw; DWORD t0; int f = 0, b; void *p;
    M4 v = trans(0, 0, 8), pr = proj();

    if (!dll) die("LoadLibrary d3d9", E_FAIL);
    create = (void *)GetProcAddress(dll, "Direct3DCreate9");
    d3d = create(D3D_SDK_VERSION);
    hw = mkwin();
    memset(&pp, 0, sizeof(pp));
    pp.BackBufferWidth = W; pp.BackBufferHeight = H; pp.BackBufferCount = 1; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow = hw; pp.Windowed = TRUE;
    pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    hr = IDirect3D9_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
    printf("CUBE: d3d9 CreateDevice hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr)) die("CreateDevice", hr);

    if (FAILED(hr = IDirect3DDevice9_CreateVertexBuffer(dev, sizeof(vtx), 0, D3DFVF_XYZ | D3DFVF_NORMAL, D3DPOOL_MANAGED, &vb, NULL))) die("VB", hr);
    IDirect3DVertexBuffer9_Lock(vb, 0, 0, &p, 0); memcpy(p, vtx, sizeof(vtx)); IDirect3DVertexBuffer9_Unlock(vb);
    if (FAILED(hr = IDirect3DDevice9_CreateIndexBuffer(dev, sizeof(idx), 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ib, NULL))) die("IB", hr);
    IDirect3DIndexBuffer9_Lock(ib, 0, 0, &p, 0); memcpy(p, idx, sizeof(idx)); IDirect3DIndexBuffer9_Unlock(ib);

    memset(&lt, 0, sizeof(lt));
    lt.Type = D3DLIGHT_DIRECTIONAL; lt.Diffuse.r = lt.Diffuse.g = lt.Diffuse.b = 1; lt.Specular.r = lt.Specular.g = lt.Specular.b = 1;
    lt.Direction.x = 0.3f; lt.Direction.y = -0.5f; lt.Direction.z = 0.8f;
    IDirect3DDevice9_SetLight(dev, 0, &lt);
    IDirect3DDevice9_LightEnable(dev, 0, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_AMBIENT, D3DCOLOR_XRGB(64, 64, 64));
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, D3DZB_TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SPECULARENABLE, TRUE);
    IDirect3DDevice9_SetTransform(dev, D3DTS_VIEW, (D3DMATRIX *)&v);
    IDirect3DDevice9_SetTransform(dev, D3DTS_PROJECTION, (D3DMATRIX *)&pr);
    IDirect3DDevice9_SetStreamSource(dev, 0, vb, 0, sizeof(Vtx));
    IDirect3DDevice9_SetIndices(dev, ib);
    IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZ | D3DFVF_NORMAL);

    t0 = GetTickCount();
    while (GetTickCount() - t0 < (DWORD)secs * 1000) {
        float t = f * 0.04f;
        IDirect3DDevice9_Clear(dev, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(128, 128, 128), 1.0f, 0);
        IDirect3DDevice9_BeginScene(dev);
        for (b = 0; b < 4; b++) {
            M4 w = world(b, t);
            memset(&mt, 0, sizeof(mt));
            mt.Diffuse.r = mt.Ambient.r = col[b][0]; mt.Diffuse.g = mt.Ambient.g = col[b][1];
            mt.Diffuse.b = mt.Ambient.b = col[b][2]; mt.Diffuse.a = mt.Ambient.a = 1;
            mt.Specular.r = mt.Specular.g = mt.Specular.b = 0.4f; mt.Power = 32;
            IDirect3DDevice9_SetMaterial(dev, &mt);
            IDirect3DDevice9_SetTransform(dev, D3DTS_WORLD, (D3DMATRIX *)&w);
            IDirect3DDevice9_DrawIndexedPrimitive(dev, D3DPT_TRIANGLELIST, 0, 0, NV, 0, NI / 3);
        }
        IDirect3DDevice9_EndScene(dev);
        hr = IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL);
        if (f < 10 || f % 30 == 0) printf("CUBE: d3d9 Present frame=%d hr=0x%08lx\n", f, (unsigned long)hr);
        f++;
        pump(10);
    }
    printf("CUBE: PASS api=d3d9 frames=%d (check pixels)\n", f);
    fflush(stdout);
    ExitProcess(0);
}

#endif

#ifdef DXCUBE_D3D8
/* ------------------------------------------------------------------ d3d8 */
static void run8(int secs)
{
    HMODULE dll = LoadLibraryA("d3d8.dll");
    IDirect3D8 *(WINAPI *create)(UINT);
    IDirect3D8 *d3d; IDirect3DDevice8 *dev = NULL; IDirect3DVertexBuffer8 *vb; IDirect3DIndexBuffer8 *ib;
    D3DPRESENT_PARAMETERS pp; D3DLIGHT8 lt; D3DMATERIAL8 mt; HRESULT hr; HWND hw; DWORD t0; int f = 0, b; BYTE *p;
    M4 v = trans(0, 0, 8), pr = proj();

    if (!dll) die("LoadLibrary d3d8", E_FAIL);
    create = (void *)GetProcAddress(dll, "Direct3DCreate8");
    d3d = create(220);
    hw = mkwin();
    memset(&pp, 0, sizeof(pp));
    pp.BackBufferWidth = W; pp.BackBufferHeight = H; pp.BackBufferCount = 1; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow = hw; pp.Windowed = TRUE;
    pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    hr = IDirect3D8_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
    printf("CUBE: d3d8 CreateDevice hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr)) die("CreateDevice", hr);

    if (FAILED(hr = IDirect3DDevice8_CreateVertexBuffer(dev, sizeof(vtx), 0, D3DFVF_XYZ | D3DFVF_NORMAL, D3DPOOL_MANAGED, &vb))) die("VB", hr);
    IDirect3DVertexBuffer8_Lock(vb, 0, 0, &p, 0); memcpy(p, vtx, sizeof(vtx)); IDirect3DVertexBuffer8_Unlock(vb);
    if (FAILED(hr = IDirect3DDevice8_CreateIndexBuffer(dev, sizeof(idx), 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ib))) die("IB", hr);
    IDirect3DIndexBuffer8_Lock(ib, 0, 0, &p, 0); memcpy(p, idx, sizeof(idx)); IDirect3DIndexBuffer8_Unlock(ib);

    memset(&lt, 0, sizeof(lt));
    lt.Type = D3DLIGHT_DIRECTIONAL; lt.Diffuse.r = lt.Diffuse.g = lt.Diffuse.b = 1; lt.Specular.r = lt.Specular.g = lt.Specular.b = 1;
    lt.Direction.x = 0.3f; lt.Direction.y = -0.5f; lt.Direction.z = 0.8f;
    IDirect3DDevice8_SetLight(dev, 0, &lt);
    IDirect3DDevice8_LightEnable(dev, 0, TRUE);
    IDirect3DDevice8_SetRenderState(dev, D3DRS_LIGHTING, TRUE);
    IDirect3DDevice8_SetRenderState(dev, D3DRS_AMBIENT, D3DCOLOR_XRGB(64, 64, 64));
    IDirect3DDevice8_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice8_SetRenderState(dev, D3DRS_ZENABLE, TRUE);
    IDirect3DDevice8_SetTransform(dev, D3DTS_VIEW, (D3DMATRIX *)&v);
    IDirect3DDevice8_SetTransform(dev, D3DTS_PROJECTION, (D3DMATRIX *)&pr);
    IDirect3DDevice8_SetStreamSource(dev, 0, vb, sizeof(Vtx));
    IDirect3DDevice8_SetIndices(dev, ib, 0);
    IDirect3DDevice8_SetVertexShader(dev, D3DFVF_XYZ | D3DFVF_NORMAL);

    t0 = GetTickCount();
    while (GetTickCount() - t0 < (DWORD)secs * 1000) {
        float t = f * 0.04f;
        IDirect3DDevice8_Clear(dev, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(128, 128, 128), 1.0f, 0);
        IDirect3DDevice8_BeginScene(dev);
        for (b = 0; b < 4; b++) {
            M4 w = world(b, t);
            memset(&mt, 0, sizeof(mt));
            mt.Diffuse.r = mt.Ambient.r = col[b][0]; mt.Diffuse.g = mt.Ambient.g = col[b][1];
            mt.Diffuse.b = mt.Ambient.b = col[b][2]; mt.Diffuse.a = mt.Ambient.a = 1;
            IDirect3DDevice8_SetMaterial(dev, &mt);
            IDirect3DDevice8_SetTransform(dev, D3DTS_WORLD, (D3DMATRIX *)&w);
            IDirect3DDevice8_DrawIndexedPrimitive(dev, D3DPT_TRIANGLELIST, 0, NV, 0, NI / 3);
        }
        IDirect3DDevice8_EndScene(dev);
        hr = IDirect3DDevice8_Present(dev, NULL, NULL, NULL, NULL);
        if (f < 10 || f % 30 == 0) printf("CUBE: d3d8 Present frame=%d hr=0x%08lx\n", f, (unsigned long)hr);
        f++;
        pump(10);
    }
    printf("CUBE: PASS api=d3d8 frames=%d (check pixels)\n", f);
    fflush(stdout);
    ExitProcess(0);
}

#endif

int main(int argc, char **argv)
{
    const char *api = argc > 1 ? argv[1] : "d3d11";
    int secs = envi("DXCUBE_SECS", 25);
    W = envi("DXCUBE_W", W); H = envi("DXCUBE_H", H);
    setvbuf(stdout, NULL, _IONBF, 0);
    mkmesh();
#ifdef DXCUBE_D3D8
    run8(secs);
#else
    if (!strcmp(api, "d3d11")) run11(secs);
    else if (!strcmp(api, "d3d10")) run10(secs);
    else if (!strcmp(api, "d3d9")) run9(secs);
#endif
    printf("CUBE: usage dxcube.exe <d3d11|d3d10|d3d9|d3d8>\n");
    return 2;
}
