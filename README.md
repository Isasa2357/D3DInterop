# D3DInterop

`D3DInterop` は、`D3D11Helper` と `D3D12Helper` の上位に位置する Direct3D 11 / Direct3D 12 相互運用ライブラリです。
同一アダプタ上で共有フェンスと共有 Texture2D を使い、D3D11 と D3D12 の GPU タイムラインとリソースを接続することを目的としています。

現在の実装段階は **P12 validated path** です。

- `SharedFence`
- `D3D11FenceEndpoint`
- `D3D12FenceEndpoint`
- `SharedTexture`
- `D3D11TextureEndpoint`
- `D3D12TextureEndpoint`
- `D3D11ToD3D12TextureChannel`
- `D3D11ToD3D12TextureRingChannel`
- `D3D11ToD3D11KeyedMutexChannel`
- `D3D11ToD3D11KeyedMutexRingChannel`
- D3D12 typed Texture2D SRV / RTV / UAV helper
- typeless resource から typed SRV / RTV / UAV を作る既定 format policy
- D3D11 allocator → D3D12 opener の NV12 / P010 / YUY2 video format view test
- D3D11 allocator → D3D12 opener の NV12 → RGBA compute shader sample
- D3D11 ⇔ D3D12 の shared fence roundtrip test
- D3D11 allocator → D3D12 opener の shared texture roundtrip test
- D3D11 allocator → D3D12 opener の reusable texture channel test
- D3D11 allocator → D3D12 opener の multi-slot ring channel test
- D3D11 allocator → D3D12 opener の GPU wait / no-readback sample
- D3D11 ⇔ D3D11 KeyedMutex の最小 test
- D3D11 ⇔ D3D11 KeyedMutex channel / ring channel test
- D3D11 allocator → D3D12 opener の typed SRV / RTV / UAV helper test
- D3D12 allocator → D3D11 opener の API-level rejection test
- install / package 用 CMake
- install 後 `find_package(D3DInterop CONFIG REQUIRED)` で別 consumer project から使えることを確認する CTest
- `FetchContent` による `D3D11Helper` / `D3D12Helper` 取得

注意: `D3D12 allocator → D3D11 opener` は一部環境で `ID3D11Device1::OpenSharedResource1` が `E_INVALIDARG` を返すことが確認されています。
そのため、現時点の安定経路は `D3D11 allocator → D3D12 opener` です。
つまり、**D3D12 で作成した shared Texture2D を D3D11 側で開き、D3D11 texture として扱う経路は現状サポートしません**。
この経路は `D3D11TextureEndpoint::Open()` の入口で明示的に拒否されます。

---

## 依存ライブラリ

既定では CMake の `FetchContent` により、以下のリポジトリを取得します。

- <https://github.com/Isasa2357/D3D11Helper.git>
- <https://github.com/Isasa2357/D3D12Helper.git>

ローカルに checkout 済みの `D3D11Helper` / `D3D12Helper` を使うこともできます。

---

## 対応環境

- Windows
- MSVC / Visual Studio
- C++17
- Direct3D 11.4
- Direct3D 12
- DXGI 1.6
- CMake 3.20 以上

---

## ビルド方法: GitHub から Helper を取得する場合

`D3DInterop` リポジトリ直下で、CMD プロンプトに以下を貼り付けて実行してください。

```bat
rmdir /s /q out\build\default 2>nul

cmake -S . -B out\build\default ^
  -DD3DINTEROP_BUILD_TESTS=ON ^
  -DD3DINTEROP_BUILD_SAMPLES=ON

cmake --build out\build\default --config Debug

ctest --test-dir out\build\default -C Debug --output-on-failure
```

---

## ビルド方法: ローカルの Helper を使う場合

次のような配置を想定します。

```text
D3DHelperShared/
  D3D11Helper/
  D3D12Helper/
  D3DInterop/
```

`D3DInterop` リポジトリ直下で、CMD プロンプトに以下を貼り付けて実行してください。

```bat
set "D3D11HELPER_ROOT=%CD%\..\D3D11Helper"
set "D3D12HELPER_ROOT=%CD%\..\D3D12Helper"

rmdir /s /q out\build\default 2>nul

cmake -S . -B out\build\default ^
  -DD3DINTEROP_D3D11HELPER_ROOT="%D3D11HELPER_ROOT%" ^
  -DD3DINTEROP_D3D12HELPER_ROOT="%D3D12HELPER_ROOT%" ^
  -DD3DINTEROP_BUILD_TESTS=ON ^
  -DD3DINTEROP_BUILD_SAMPLES=ON

cmake --build out\build\default --config Debug

ctest --test-dir out\build\default -C Debug --output-on-failure
```

---

## インストール

`D3DInterop` は `cmake --install` に対応しています。既定では install rule が有効です。

```bat
rmdir /s /q out\build\install 2>nul
rmdir /s /q out\install\D3DInterop 2>nul

cmake -S . -B out\build\install ^
  -DD3DINTEROP_BUILD_TESTS=OFF ^
  -DD3DINTEROP_BUILD_SAMPLES=OFF ^
  -DD3DINTEROP_INSTALL=ON

cmake --build out\build\install --config Release

cmake --install out\build\install --config Release --prefix out\install\D3DInterop
```

install 後は、別プロジェクトから次のように使えます。

```cmake
find_package(D3DInterop CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE D3DInterop::D3DInterop)
```

package config は、`D3D11Helper::D3D11Helper` / `D3D12Helper::D3D12Helper` が未定義の場合、既定では `FetchContent` で helper を取得します。
この挙動を止めたい場合は、利用側プロジェクトで `D3DINTEROP_PACKAGE_FETCH_HELPERS=OFF` を指定し、helper package / target を先に用意してください。

---

## install 後 consumer test

`InstalledPackageConsumer` CTest は、現在の build tree から一度 `cmake --install` を実行し、別ディレクトリの最小 consumer project を configure / build / test します。
この consumer は `find_package(D3DInterop CONFIG REQUIRED)` と `target_link_libraries(... D3DInterop::D3DInterop)` だけで D3DInterop を利用します。

この確認を無効化したい場合は、configure 時に次を指定してください。

```bat
-DD3DINTEROP_BUILD_INSTALLED_PACKAGE_TEST=OFF
```

---

## ZIP パッケージ作成

CPack による ZIP package を作れます。

```bat
rmdir /s /q out\build\package 2>nul

cmake -S . -B out\build\package ^
  -DD3DINTEROP_BUILD_TESTS=OFF ^
  -DD3DINTEROP_BUILD_SAMPLES=OFF ^
  -DD3DINTEROP_INSTALL=ON ^
  -DD3DINTEROP_ENABLE_CPACK=ON

cmake --build out\build\package --config Release

cmake --build out\build\package --config Release --target package
```

---

## 基本設計

### レイヤ構成

```text
Application / Upper subsystem
          |
          v
D3DInterop
  - SharedFence
  - D3D11FenceEndpoint
  - D3D12FenceEndpoint
  - SharedTexture
  - D3D11TextureEndpoint
  - D3D12TextureEndpoint
  - D3D11ToD3D12TextureChannel
  - D3D11ToD3D12TextureRingChannel
  - D3D11ToD3D11KeyedMutexChannel
  - D3D11ToD3D11KeyedMutexRingChannel
  - D3D12TextureViewHelpers
          |
          +---- D3D11Helper
          |
          +---- D3D12Helper
```

`D3D11Helper` と `D3D12Helper` は互いを直接参照しません。
D3D11 と D3D12 の両方を知るのは `D3DInterop` のみです。

### 同期モデル

SharedFence 系では、Producer は共有対象への GPU 書き込みを積んだ後に `Signal(value)` します。
Consumer は共有対象を読む前に `GpuWait(value)` または `CpuWait(value)` します。

```text
Producer queue/context                 Consumer queue/context
----------------------                 ----------------------
write resource
Signal(fence, N)      -------------->  Wait(fence, N)
                                       read resource
```

KeyedMutex 系では、各 texture slot を次の key protocol で回します。

```text
D3D11 producer: Acquire(0) -> write -> Release(1)
D3D11 consumer: Acquire(1) -> read  -> Release(0)
```

### D3D12 typed view helper

D3D11 側で作成した shared texture を D3D12 側で SRV / RTV / UAV として扱う場合、D3DInterop は descriptor heap 自体は所有しません。
代わりに、アプリケーションが用意した `D3D12_CPU_DESCRIPTOR_HANDLE` に対して typed view descriptor を作成する helper を提供します。

```cpp
D3DInteropLib::D3D12Texture2DSrvOptions srv;
srv.format = DXGI_FORMAT_R8G8B8A8_UNORM;
D3DInteropLib::CreateD3D12Texture2DSrv(device, endpoint12, srvHandle, srv);

D3DInteropLib::D3D12Texture2DRtvOptions rtv;
rtv.format = DXGI_FORMAT_R8G8B8A8_UNORM;
D3DInteropLib::CreateD3D12Texture2DRtv(device, endpoint12, rtvHandle, rtv);

D3DInteropLib::D3D12Texture2DUavOptions uav;
uav.format = DXGI_FORMAT_R8G8B8A8_UNORM;
D3DInteropLib::CreateD3D12Texture2DUav(device, endpoint12, uavHandle, uav);
```

`DXGI_FORMAT_R8G8B8A8_TYPELESS` などの typeless resource では、既定の typed view format policy により `DXGI_FORMAT_R8G8B8A8_UNORM` を SRV / RTV / UAV の既定値として使えます。
詳細は `docs/TYPED_FORMAT_VIEWS.md` を参照してください。

NV12 / P010 などの multi-plane format では、plane ごとに typed SRV を作ります。

```cpp
// NV12 Y plane
D3DInteropLib::D3D12Texture2DSrvOptions y;
y.format = D3DInteropLib::GetD3DInteropVideoPlaneSrvFormat(DXGI_FORMAT_NV12, 0);
y.planeSlice = 0;

// NV12 UV plane
D3DInteropLib::D3D12Texture2DSrvOptions uv;
uv.format = D3DInteropLib::GetD3DInteropVideoPlaneSrvFormat(DXGI_FORMAT_NV12, 1);
uv.planeSlice = 1;
```

### 現在の安定 texture quadrant

現時点でテスト済みの D3D11 / D3D12 間 texture quadrant は次です。

```text
D3D11 allocator -> D3D12 opener
```

D3D11 / D3D11 については、KeyedMutex channel / ring channel を提供します。
詳細は `docs/QUADRANT_MATRIX.md` を参照してください。

---

## 現在のテスト / サンプル

CTest には以下が登録されます。

- `FenceRoundTrip`
- `SharedTexture11To12`
- `TextureChannel11To12`
- `TextureRingChannel11To12`
- `KeyedMutexD3D11ToD3D11`
- `KeyedMutexChannelD3D11ToD3D11`
- `KeyedMutexRingChannelD3D11ToD3D11`
- `TypedSrv11To12`
- `Nv12VideoTexture11To12`
- `RenderTargetWrite11To12`
- `UnorderedAccessWrite11To12`
- `VideoFormatViews11To12`
- `TypelessViewPolicy11To12`
- `UnsupportedD3D12ToD3D11`
- `InstalledPackageConsumer`
- `PingPongFeedback11To12`
- `GpuWait11To12`
- `RingAsyncNoReadback11To12`
- `Nv12ToRgbaCompute11To12`

実行:

```bat
ctest --test-dir out\build\default -C Debug --output-on-failure
```

---

## ディレクトリ構成

```text
D3DInterop/
  include/D3DInterop/
    D3DInterop.hpp
    D3DInteropCommon.hpp
    SharedFence.hpp
    SharedTexture.hpp
    D3D11Endpoint.hpp
    D3D12Endpoint.hpp
    D3D11ToD3D12TextureChannel.hpp
    D3D11ToD3D12TextureRingChannel.hpp
    D3D11ToD3D11KeyedMutexChannel.hpp
    D3D11ToD3D11KeyedMutexRingChannel.hpp
    D3D12TextureViewHelpers.hpp
  src/
    SharedFence.cpp
    SharedTexture.cpp
    D3D11Endpoint.cpp
    D3D11TextureEndpoint.cpp
    D3D12Endpoint.cpp
    D3D12TextureEndpoint.cpp
    D3D11ToD3D12TextureChannel.cpp
    D3D11ToD3D12TextureRingChannel.cpp
    D3D11ToD3D11KeyedMutexChannel.cpp
    D3D11ToD3D11KeyedMutexRingChannel.cpp
    D3D12TextureViewHelpers.cpp
  sample/
    nv12_to_rgba_compute_11_to_12.cpp
  test/
    package_consumer/
      CMakeLists.txt
      main.cpp
  docs/
    QUADRANT_MATRIX.md
    TYPED_FORMAT_VIEWS.md
  cmake/
    D3DInteropConfig.cmake.in
    RunInstalledPackageConsumerTest.cmake
  CMakeLists.txt
  README.md
  .gitignore
```

---

## 実装フェーズ

| Phase | 内容 | 状態 |
| --- | --- | --- |
| P0 | `D3D12Fence` shared handle 対応 | 実装済み |
| P1 | `SharedFence` / D3D11・D3D12 fence endpoint / fence roundtrip test | 実装済み |
| P2 | `SharedTexture` / texture endpoint / 1 象限 texture handoff | `D3D11 -> D3D12` で実装済み |
| P3 | available quadrant matrix の定義 / unsupported quadrant の明示 | 実装済み |
| P4 | ping-pong / feedback loop sample | `D3D11 -> D3D12` validated path で実装済み |
| P5 | reusable texture channel API | `D3D11ToD3D12TextureChannel` として実装済み |
| P6 | multi-slot texture ring channel API | `D3D11ToD3D12TextureRingChannel` として実装済み |
| P7 | unsupported quadrant API-level rejection / install / package CMake | 実装済み |
| P8 | installed package external consumer test | 実装済み |
| P9 | D3D11 / D3D11 KeyedMutex channel / ring channel API | 実装済み |
| P10 | D3D11 -> D3D12 typed SRV helper / NV12 video texture test | 実装済み |
| P11 | D3D11 -> D3D12 typed RTV / UAV helper and write tests | 実装済み |
| P12 | NV12 -> RGBA compute sample / P010・YUY2 view tests / typeless view policy | 実装済み |

---

## ライセンス

未定です。GitHub 公開時には、必要に応じて `LICENSE` を追加してください。
