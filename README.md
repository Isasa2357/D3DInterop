# D3DInterop

`D3DInterop` は、`D3D11Helper` と `D3D12Helper` の上位に位置する Direct3D 11 / Direct3D 12 相互運用ライブラリです。
同一アダプタ上で共有フェンスと共有 Texture2D を使い、D3D11 と D3D12 の GPU タイムラインとリソースを接続することを目的としています。

現在の実装段階は **P5 validated path** です。

- `SharedFence`
- `D3D11FenceEndpoint`
- `D3D12FenceEndpoint`
- `SharedTexture`
- `D3D11TextureEndpoint`
- `D3D12TextureEndpoint`
- `D3D11ToD3D12TextureChannel`
- D3D11 ⇔ D3D12 の shared fence roundtrip test
- D3D11 allocator → D3D12 opener の shared texture roundtrip test
- D3D11 allocator → D3D12 opener の ping-pong / feedback loop sample
- D3D11 allocator → D3D12 opener の reusable texture channel test
- `FetchContent` による `D3D11Helper` / `D3D12Helper` 取得

注意: `D3D12 allocator → D3D11 opener` は一部環境で `ID3D11Device1::OpenSharedResource1` が `E_INVALIDARG` を返すことが確認されています。
そのため、現時点の安定経路は `D3D11 allocator → D3D12 opener` です。
つまり、**D3D12 で作成した shared Texture2D を D3D11 側で開き、D3D11 texture として扱う経路は現状サポートしません**。

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
          |
          +---- D3D11Helper
          |
          +---- D3D12Helper
```

`D3D11Helper` と `D3D12Helper` は互いを直接参照しません。
D3D11 と D3D12 の両方を知るのは `D3DInterop` のみです。

### 同期モデル

Producer は共有対象への GPU 書き込みを積んだ後に `Signal(value)` します。
Consumer は共有対象を読む前に `GpuWait(value)` または `CpuWait(value)` します。

```text
Producer queue/context                 Consumer queue/context
----------------------                 ----------------------
write resource
Signal(fence, N)      -------------->  Wait(fence, N)
                                       read resource
```

Fence value は上位プロトコルが単調増加で管理します。

### 現在の安定 texture quadrant

現時点でテスト済みの texture quadrant は次です。

```text
D3D11 allocator -> D3D12 opener
```

### `D3D11ToD3D12TextureChannel`

`D3D11ToD3D12TextureChannel` は、現在の validated path をアプリケーションから使いやすくする高レベル API です。

```text
D3D11 producer writes frame N
D3D11 producer signals readyFence(N)
D3D12 consumer waits readyFence(N)
D3D12 consumer reads frame N
D3D12 consumer signals consumedFence(N)
D3D11 producer waits consumedFence(N) before overwriting
```

アプリケーションは `ProducerTexture()` で取得した D3D11 texture に書き込み、`ConsumerTexture()` で取得した D3D12 resource を読むだけで、ready / consumed の 2 本の shared fence による ping-pong 制御を利用できます。

---

## 現在のテスト / サンプル

CTest には以下が登録されます。

- `FenceRoundTrip`
- `SharedTexture11To12`
- `TextureChannel11To12`
- `PingPongFeedback11To12`

実行:

```bat
ctest --test-dir out\build\default -C Debug --output-on-failure
```

サンプル単体実行:

```bat
out\build\default\sample\Debug\sample_ping_pong_feedback_11_to_12.exe
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
  src/
    SharedFence.cpp
    SharedTexture.cpp
    D3D11Endpoint.cpp
    D3D11TextureEndpoint.cpp
    D3D12Endpoint.cpp
    D3D12TextureEndpoint.cpp
    D3D11ToD3D12TextureChannel.cpp
  sample/
    CMakeLists.txt
    ping_pong_feedback_11_to_12.cpp
  test/
    CMakeLists.txt
    test_fence_roundtrip.cpp
    test_shared_texture_11_to_12.cpp
    test_texture_channel_11_to_12.cpp
  docs/
    QUADRANT_MATRIX.md
  CMakeLists.txt
  README.md
  .gitignore
```

---

## Git に入れないもの

`.gitignore` により、以下は無視されます。

- `out/`
- `build/`
- CMake の生成物
- Visual Studio の生成物
- 一時ファイル
- バイナリ / オブジェクトファイル

そのため、ビルド後でも基本的に次を実行できます。

```bat
git add .
git status
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

---

## ライセンス

未定です。GitHub 公開時には、必要に応じて `LICENSE` を追加してください。
