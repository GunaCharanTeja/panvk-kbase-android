# DX9/10/11 sample exes

Microsoft DirectX SDK (June 2010) prebuilt samples + llvm-mingw tutorials. Binaries are gitignored (not committed); only this README and INDEX.tsv are tracked. List: INDEX.tsv.

Layout: `{dx9,dx10,dx11}/{x86,x64}/*.exe`, `{dx9,dx11}/tutorials-{i686,x86_64}/`, `redist/{x86,x64}/` (d3dx9_43, d3dx10_43, d3dx11_43, D3DCompiler_43 DLLs).

## Media (NOT copied, 506 MB)

Read from `/var/tmp/panvk/dxtests/DXSDK/DXSDK/Samples/Media`. Prebuilt SDK samples (DXUT) find media by relative path, roughly `..\..\Media` from the exe dir in the original SDK tree (`Samples/C++/Direct3D*/Bin/{x86,x64}`). Copies here break that. Options: run the exes from the original SDK `Bin` dirs, or symlink/mount `Samples/Media` so `../../Media` resolves from the exe dir. All SDK-prebuilt exes are marked needs-media=yes in INDEX.tsv.

Tutorials: Tut01-04 and dx11 Tutorial01 need none. Tut05 needs `banana.bmp`, Tut06 needs `Tiger.x` + `tiger.bmp`; take them from the Media dir above and place next to the exe (not copied).

## DLLs

Put `redist/<arch>/*.dll` next to the exe (or install in the Wine prefix). x86 exes use `redist/x86`, x64 use `redist/x64`.
