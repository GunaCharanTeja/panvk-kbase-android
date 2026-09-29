#include <cstdio>
#include <cstdint>
#include <cstring>

#include <SDL.h>
#include <d3d9.h>
#include <d3d11.h>

static int probe_d3d9(SDL_Window *window, bool workload, bool do_present) {
  IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
  if (!d3d)
    return 10;

  D3DPRESENT_PARAMETERS present = {};
  present.Windowed = TRUE;
  present.SwapEffect = D3DSWAPEFFECT_DISCARD;
  present.hDeviceWindow = reinterpret_cast<HWND>(window);

  IDirect3DDevice9 *device = nullptr;
  const HRESULT hr = d3d->CreateDevice(
      D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, present.hDeviceWindow,
      D3DCREATE_HARDWARE_VERTEXPROCESSING, &present, &device);
  std::printf("D3D9 HRESULT=0x%08x\n", static_cast<unsigned>(hr));
  if (SUCCEEDED(hr) && workload) {
    IDirect3DSurface9 *back = nullptr;
    IDirect3DSurface9 *readback = nullptr;
    D3DSURFACE_DESC desc = {};
    D3DLOCKED_RECT locked = {};
    HRESULT work = device->Clear(0, nullptr, D3DCLEAR_TARGET,
                                 D3DCOLOR_XRGB(0, 0, 255), 1.0f, 0);
    struct Vertex {
      float x, y, z, rhw;
      D3DCOLOR color;
    };
    const Vertex triangle[3] = {
        {100.0f, 100.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 0, 0)},
        {540.0f, 100.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 0, 0)},
        {320.0f, 440.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 0, 0)},
    };
    if (SUCCEEDED(work))
      work = device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    if (SUCCEEDED(work))
      work = device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    if (SUCCEEDED(work))
      work = device->BeginScene();
    if (SUCCEEDED(work)) {
      const HRESULT draw = device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1,
                                                   triangle, sizeof(Vertex));
      const HRESULT end = device->EndScene();
      work = FAILED(draw) ? draw : end;
    }
    if (SUCCEEDED(work))
      work = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back);
    if (SUCCEEDED(work))
      work = back->GetDesc(&desc);
    if (SUCCEEDED(work))
      work = device->CreateOffscreenPlainSurface(desc.Width, desc.Height,
          desc.Format, D3DPOOL_SYSTEMMEM, &readback, nullptr);
    if (SUCCEEDED(work))
      work = device->GetRenderTargetData(back, readback);
    if (SUCCEEDED(work))
      work = readback->LockRect(&locked, nullptr, D3DLOCK_READONLY);
    bool pixels_ok = false;
    if (SUCCEEDED(work)) {
      const auto *px = static_cast<const uint8_t *>(locked.pBits);
      const auto *center = px + (desc.Height / 2) * locked.Pitch +
                           (desc.Width / 2) * 4;
      pixels_ok = px[0] >= 250 && px[1] <= 5 && px[2] <= 5 &&
                  center[0] <= 5 && center[1] <= 5 && center[2] >= 250;
      readback->UnlockRect();
    }
    if (SUCCEEDED(work) && pixels_ok && do_present)
      work = device->Present(nullptr, nullptr, nullptr, nullptr);
    std::printf("D3D9_WORKLOAD HRESULT=0x%08x triangle_pixel=%d\n",
                static_cast<unsigned>(work), pixels_ok);
    std::fflush(stdout);
    if (readback)
      readback->Release();
    if (back)
      back->Release();
    if (FAILED(work) || !pixels_ok) {
      if (device)
        device->Release();
      d3d->Release();
      return 12;
    }
  }
  if (device)
    device->Release();
  d3d->Release();
  return FAILED(hr) ? 11 : 0;
}

static int probe_d3d11(bool workload) {
  const D3D_FEATURE_LEVEL requested[] = {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
  };
  ID3D11Device *device = nullptr;
  ID3D11DeviceContext *context = nullptr;
  D3D_FEATURE_LEVEL returned = {};
  const HRESULT hr = D3D11CreateDevice(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, requested,
      sizeof(requested) / sizeof(requested[0]), D3D11_SDK_VERSION, &device,
      &returned, &context);
  std::printf("D3D11 HRESULT=0x%08x feature_level=0x%04x\n",
              static_cast<unsigned>(hr), static_cast<unsigned>(returned));
  if (SUCCEEDED(hr) && workload) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = 64;
    desc.Height = 64;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D *rt = nullptr;
    ID3D11Texture2D *readback = nullptr;
    ID3D11RenderTargetView *rtv = nullptr;
    HRESULT work = device->CreateTexture2D(&desc, nullptr, &rt);
    if (SUCCEEDED(work))
      work = device->CreateRenderTargetView(rt, nullptr, &rtv);
    if (SUCCEEDED(work)) {
      const float color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
      context->ClearRenderTargetView(rtv, color);
      desc.Usage = D3D11_USAGE_STAGING;
      desc.BindFlags = 0;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      work = device->CreateTexture2D(&desc, nullptr, &readback);
    }
    if (SUCCEEDED(work)) {
      context->CopyResource(readback, rt);
      D3D11_MAPPED_SUBRESOURCE mapped = {};
      work = context->Map(readback, 0, D3D11_MAP_READ, 0, &mapped);
      if (SUCCEEDED(work)) {
        const auto *px = static_cast<const uint8_t *>(mapped.pData);
        if (px[0] < 250 || px[1] > 5 || px[2] > 5 || px[3] < 250)
          work = E_FAIL;
        context->Unmap(readback, 0);
      }
    }
    std::printf("D3D11_WORKLOAD HRESULT=0x%08x red_pixel=%d\n",
                static_cast<unsigned>(work), SUCCEEDED(work));
    std::fflush(stdout);
    if (rtv)
      rtv->Release();
    if (readback)
      readback->Release();
    if (rt)
      rt->Release();
    if (FAILED(work)) {
      context->Release();
      device->Release();
      return 21;
    }
  }
  if (context)
    context->Release();
  if (device)
    device->Release();
  return FAILED(hr) ? 20 : 0;
}

int main(int argc, char **argv) {
  if (argc != 2 || (std::strcmp(argv[1], "d3d9") != 0 &&
                    std::strcmp(argv[1], "d3d9-workload") != 0 &&
                    std::strcmp(argv[1], "d3d9-present") != 0 &&
                    std::strcmp(argv[1], "d3d11") != 0 &&
                    std::strcmp(argv[1], "d3d11-workload") != 0 &&
                    std::strcmp(argv[1], "d3d11-headless") != 0)) {
    std::fprintf(stderr, "usage: %s d3d9|d3d9-workload|d3d9-present|d3d11|d3d11-workload|d3d11-headless\n", argv[0]);
    return 2;
  }
  if (std::strcmp(argv[1], "d3d11-headless") == 0) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
      return 3;
    }
    const int result = probe_d3d11(false);
    SDL_Quit();
    return result;
  }
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 3;
  }
  SDL_Window *window = SDL_CreateWindow("DXVK Native probe",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480,
      SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
  if (!window) {
    std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
    SDL_Quit();
    return 4;
  }
  const bool d3d9 = std::strncmp(argv[1], "d3d9", 4) == 0;
  const bool present = std::strcmp(argv[1], "d3d9-present") == 0;
  const bool workload = present || std::strstr(argv[1], "workload") != nullptr;
  const int result = d3d9 ? probe_d3d9(window, workload, present)
                          : probe_d3d11(workload);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return result;
}
