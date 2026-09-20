#include <cstdio>
#include <cstring>

#include <SDL.h>
#include <d3d9.h>
#include <d3d11.h>

static int probe_d3d9(SDL_Window *window) {
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
  if (device)
    device->Release();
  d3d->Release();
  return FAILED(hr) ? 11 : 0;
}

static int probe_d3d11() {
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
  if (context)
    context->Release();
  if (device)
    device->Release();
  return FAILED(hr) ? 20 : 0;
}

int main(int argc, char **argv) {
  if (argc != 2 || (std::strcmp(argv[1], "d3d9") != 0 &&
                    std::strcmp(argv[1], "d3d11") != 0 &&
                    std::strcmp(argv[1], "d3d11-headless") != 0)) {
    std::fprintf(stderr, "usage: %s d3d9|d3d11|d3d11-headless\n", argv[0]);
    return 2;
  }
  if (std::strcmp(argv[1], "d3d11-headless") == 0)
    return probe_d3d11();
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
  const int result = std::strcmp(argv[1], "d3d9") == 0
      ? probe_d3d9(window) : probe_d3d11();
  SDL_DestroyWindow(window);
  SDL_Quit();
  return result;
}
