# D3D9/10/11 sample sources (no binaries in repo)

Binaries live in `/var/tmp/panvk/dxtests/` (see `INDEX.md` there).

- Microsoft DirectX SDK June 2010 (`DXSDK_Jun10.exe`, official MS download, 599 MB): prebuilt x86+x64 samples with shipped media.
  - D3D9 (32): BasicHLSL, SimpleSample, Instancing, HDRFormats, HDRLighting, ShadowMap, ShadowVolume, SkinnedMesh, PostProcess, DepthOfField, ...
  - D3D10 (34): SimpleSample10, BasicHLSL10, ParticlesGS, CubeMapGS, FixedFuncEMU, SoftParticles, Instancing10, SubD10, ...
  - D3D11 (24): SimpleSample11, BasicHLSL11, EmptyProject11, SubD11, PNTriangles11, DetailTessellation11, OIT11, NBodyGravityCS11, HDRToneMappingCS11, ...
- Source mirror (not built, MSVC): https://github.com/walbourn/directx-sdk-samples @ 1ad8f0f6a3e4d9be7e54ca52640ac12b6565ab0c
- Built with llvm-mingw (x86_64 + i686): SDK D3D9 Tutorials Tut01-Tut06 and D3D11 Tutorial01.
- arm64ec: none (D3DX is x86/x64 only); run x64 builds emulated.
