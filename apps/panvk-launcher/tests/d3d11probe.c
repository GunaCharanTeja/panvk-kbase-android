/*
 * Build line:
 *   /var/tmp/panvk/launcher-tests/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64/bin/<triple>-clang -O2 d3d11probe.c -o d3d11probe.exe -ld3d11 -ldxgi -luuid
 *
 * where <triple> is one of:
 *   - aarch64-w64-mingw32
 *   - arm64ec-w64-mingw32
 *   - x86_64-w64-mingw32
 */

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    ID3D11Device *pDevice = NULL;
    ID3D11DeviceContext *pContext = NULL;
    D3D_FEATURE_LEVEL featureLevel = (D3D_FEATURE_LEVEL)0;

    HRESULT hr = D3D11CreateDevice(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        0,
        NULL,
        0,
        D3D11_SDK_VERSION,
        &pDevice,
        &featureLevel,
        &pContext
    );
    if (FAILED(hr) || !pDevice || !pContext) {
        printf("D3D11PROBE: FAIL D3D11CreateDevice failed (0x%08lx)\n", (unsigned long)hr);
        fflush(stdout);
        return 1;
    }

    IDXGIDevice *pDXGIDevice = NULL;
    hr = pDevice->lpVtbl->QueryInterface(pDevice, &IID_IDXGIDevice, (void **)&pDXGIDevice);
    if (FAILED(hr) || !pDXGIDevice) {
        printf("D3D11PROBE: FAIL QueryInterface(IDXGIDevice) failed (0x%08lx)\n", (unsigned long)hr);
        fflush(stdout);
        pContext->lpVtbl->Release(pContext);
        pDevice->lpVtbl->Release(pDevice);
        return 1;
    }

    IDXGIAdapter *pAdapter = NULL;
    hr = pDXGIDevice->lpVtbl->GetAdapter(pDXGIDevice, &pAdapter);
    if (FAILED(hr) || !pAdapter) {
        printf("D3D11PROBE: FAIL IDXGIDevice->GetAdapter failed (0x%08lx)\n", (unsigned long)hr);
        fflush(stdout);
        pDXGIDevice->lpVtbl->Release(pDXGIDevice);
        pContext->lpVtbl->Release(pContext);
        pDevice->lpVtbl->Release(pDevice);
        return 1;
    }

    DXGI_ADAPTER_DESC desc;
    memset(&desc, 0, sizeof(desc));
    hr = pAdapter->lpVtbl->GetDesc(pAdapter, &desc);
    if (FAILED(hr)) {
        printf("D3D11PROBE: FAIL IDXGIAdapter->GetDesc failed (0x%08lx)\n", (unsigned long)hr);
        fflush(stdout);
        pAdapter->lpVtbl->Release(pAdapter);
        pDXGIDevice->lpVtbl->Release(pDXGIDevice);
        pContext->lpVtbl->Release(pContext);
        pDevice->lpVtbl->Release(pDevice);
        return 1;
    }

    char descUtf8[512] = {0};
    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, descUtf8, sizeof(descUtf8), NULL, NULL);

    printf("adapter: %s\n", descUtf8);
    fflush(stdout);
    printf("feature level: 0x%04x\n", (unsigned int)featureLevel);
    fflush(stdout);

    pAdapter->lpVtbl->Release(pAdapter);
    pDXGIDevice->lpVtbl->Release(pDXGIDevice);

    D3D11_TEXTURE2D_DESC rtDesc;
    memset(&rtDesc, 0, sizeof(rtDesc));
    rtDesc.Width = 64;
    rtDesc.Height = 64;
    rtDesc.MipLevels = 1;
    rtDesc.ArraySize = 1;
    rtDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtDesc.SampleDesc.Count = 1;
    rtDesc.SampleDesc.Quality = 0;
    rtDesc.Usage = D3D11_USAGE_DEFAULT;
    rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    rtDesc.CPUAccessFlags = 0;
    rtDesc.MiscFlags = 0;

    ID3D11Texture2D *pRenderTargetTex = NULL;
    hr = pDevice->lpVtbl->CreateTexture2D(pDevice, &rtDesc, NULL, &pRenderTargetTex);
    if (FAILED(hr) || !pRenderTargetTex) {
        printf("D3D11PROBE: FAIL CreateTexture2D (render target) failed (0x%08lx)\n", (unsigned long)hr);
        fflush(stdout);
        pContext->lpVtbl->Release(pContext);
        pDevice->lpVtbl->Release(pDevice);
        return 1;
    }

    ID3D11RenderTargetView *pRTV = NULL;
    hr = pDevice->lpVtbl->CreateRenderTargetView(pDevice, (ID3D11Resource *)pRenderTargetTex, NULL, &pRTV);
    if (FAILED(hr) || !pRTV) {
        printf("D3D11PROBE: FAIL CreateRenderTargetView failed (0x%08lx)\n", (unsigned long)hr);
        fflush(stdout);
        pRenderTargetTex->lpVtbl->Release(pRenderTargetTex);
        pContext->lpVtbl->Release(pContext);
        pDevice->lpVtbl->Release(pDevice);
        return 1;
    }

    const float clearColor[4] = { 1.0f, 0.5f, 0.25f, 1.0f };
    pContext->lpVtbl->ClearRenderTargetView(pContext, pRTV, clearColor);

    D3D11_TEXTURE2D_DESC stagingDesc;
    memset(&stagingDesc, 0, sizeof(stagingDesc));
    stagingDesc.Width = 64;
    stagingDesc.Height = 64;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.SampleDesc.Quality = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;

    ID3D11Texture2D *pStagingTex = NULL;
    hr = pDevice->lpVtbl->CreateTexture2D(pDevice, &stagingDesc, NULL, &pStagingTex);
    if (FAILED(hr) || !pStagingTex) {
        printf("D3D11PROBE: FAIL CreateTexture2D (staging) failed (0x%08lx)\n", (unsigned long)hr);
        fflush(stdout);
        pRTV->lpVtbl->Release(pRTV);
        pRenderTargetTex->lpVtbl->Release(pRenderTargetTex);
        pContext->lpVtbl->Release(pContext);
        pDevice->lpVtbl->Release(pDevice);
        return 1;
    }

    pContext->lpVtbl->CopyResource(pContext, (ID3D11Resource *)pStagingTex, (ID3D11Resource *)pRenderTargetTex);

    D3D11_MAPPED_SUBRESOURCE mapped;
    memset(&mapped, 0, sizeof(mapped));
    hr = pContext->lpVtbl->Map(pContext, (ID3D11Resource *)pStagingTex, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        printf("D3D11PROBE: FAIL Map failed (0x%08lx)\n", (unsigned long)hr);
        fflush(stdout);
        pStagingTex->lpVtbl->Release(pStagingTex);
        pRTV->lpVtbl->Release(pRTV);
        pRenderTargetTex->lpVtbl->Release(pRenderTargetTex);
        pContext->lpVtbl->Release(pContext);
        pDevice->lpVtbl->Release(pDevice);
        return 1;
    }

    const uint8_t *pixel = (const uint8_t *)mapped.pData;
    uint8_t r = pixel[0];
    uint8_t g = pixel[1];
    uint8_t b = pixel[2];
    uint8_t a = pixel[3];

    printf("first pixel: %02x %02x %02x %02x\n", r, g, b, a);
    fflush(stdout);

    pContext->lpVtbl->Unmap(pContext, (ID3D11Resource *)pStagingTex, 0);

    /* Expected ~ff 80 40 ff with tolerance 1 */
    int r_diff = abs((int)r - 0xff);
    int g_diff = abs((int)g - 0x80);
    int b_diff = abs((int)b - 0x40);
    int a_diff = abs((int)a - 0xff);

    int pass = (r_diff <= 1 && g_diff <= 1 && b_diff <= 1 && a_diff <= 1);
    if (pass) {
        printf("D3D11PROBE: PASS\n");
        fflush(stdout);
    } else {
        printf("D3D11PROBE: FAIL pixel mismatch: got %02x %02x %02x %02x, expected ~ff 80 40 ff (tolerance 1)\n",
               r, g, b, a);
        fflush(stdout);
    }

    pStagingTex->lpVtbl->Release(pStagingTex);
    pRTV->lpVtbl->Release(pRTV);
    pRenderTargetTex->lpVtbl->Release(pRenderTargetTex);
    pContext->lpVtbl->Release(pContext);
    pDevice->lpVtbl->Release(pDevice);

    return pass ? 0 : 1;
}
