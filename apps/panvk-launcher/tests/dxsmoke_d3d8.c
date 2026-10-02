/*
 * d3d8 half of dxsmoke. Separate translation unit because d3d8types.h
 * and d3d9types.h redefine the same enums and cannot share one file.
 */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d8.h>
#include <stdio.h>
#include <string.h>

void done(int code);
void fail(const char *what, HRESULT hr);
void *sym(HMODULE mod, const char *name);
HWND make_window(const char *title);
void pump(DWORD ms);
DWORD hold_ms(void);
int staging_enabled(void);
void source_pixels(const char *api, const void *data, unsigned pitch);

#define W 320
#define H 240

void run_d3d8(void)
{
    HMODULE dll = LoadLibraryA("d3d8.dll");
    IDirect3D8 *(WINAPI *create)(UINT);
    IDirect3D8 *d3d;
    IDirect3DDevice8 *dev = NULL;
    IDirect3DSurface8 *bb = NULL;
    D3DLOCKED_RECT map;
    D3DPRESENT_PARAMETERS pp;
    D3DADAPTER_IDENTIFIER8 id;
    HRESULT hr;
    HWND hwnd;

    if (!dll)
        fail("LoadLibrary(d3d8.dll)", HRESULT_FROM_WIN32(GetLastError()));
    create = sym(dll, "Direct3DCreate8");
    d3d = create(220);
    if (!d3d)
        fail("Direct3DCreate8", E_FAIL);

    hwnd = make_window("dxsmoke d3d8");
    memset(&pp, 0, sizeof(pp));
    pp.BackBufferWidth = W;
    pp.BackBufferHeight = H;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = hwnd;
    pp.Windowed = TRUE;
    if (staging_enabled()) pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

    hr = IDirect3D8_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                 D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
    printf("DXSMOKE: api=d3d8 CreateDevice hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr) || !dev)
        fail("IDirect3D8::CreateDevice", hr);

    memset(&id, 0, sizeof(id));
    hr = IDirect3D8_GetAdapterIdentifier(d3d, D3DADAPTER_DEFAULT, 0, &id);
    printf("DXSMOKE: api=d3d8 adapter hr=0x%08lx desc=\"%s\" vendor=0x%04lx device=0x%04lx\n",
           (unsigned long)hr, id.Description, (unsigned long)id.VendorId, (unsigned long)id.DeviceId);

    hr = IDirect3DDevice8_Clear(dev, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 128, 64), 1.0f, 0);
    printf("DXSMOKE: api=d3d8 Clear hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr))
        fail("IDirect3DDevice8::Clear", hr);
    if (staging_enabled()) {
        hr = IDirect3DDevice8_GetBackBuffer(dev, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
        if (FAILED(hr)) fail("GetBackBuffer", hr);
        hr = IDirect3DSurface8_LockRect(bb, &map, NULL, D3DLOCK_READONLY);
        if (FAILED(hr)) fail("LockRect", hr);
        source_pixels("d3d8", map.pBits, map.Pitch);
        IDirect3DSurface8_UnlockRect(bb);
        IDirect3DSurface8_Release(bb);
    }
    hr = IDirect3DDevice8_Present(dev, NULL, NULL, NULL, NULL);
    printf("DXSMOKE: api=d3d8 Present hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr))
        fail("IDirect3DDevice8::Present", hr);

    pump(hold_ms());
    IDirect3DDevice8_Release(dev);
    IDirect3D8_Release(d3d);
    printf("DXSMOKE: PASS api=d3d8\n");
    done(0);
}
