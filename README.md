# D3DInterop

`D3DInterop` は、`D3D11Helper` と `D3D12Helper` の上位に位置する Direct3D 11 / Direct3D 12 相互運用ライブラリです。
同一アダプタ上で共有フェンスを使い、D3D11 と D3D12 の GPU タイムラインを安全に接続することを目的としています。

現在の実装段階は **P1** です。

- `SharedFence`
- `D3D11FenceEndpoint`
- `D3D12FenceEndpoint`
- D3D11 ⇔ D3D12 の shared fence roundtrip test
- `FetchContent` による `D3D11Helper` / `D3D12Helper` 取得

`SharedTexture` / `TextureEndpoint` / 12→11 texture handoff は次フェーズ以降で実装予定です。

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
  -DD3DINTEROP_BUILD_TESTS=ON

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
  -DD3DINTEROP_BUILD_TESTS=ON

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
  - SharedTexture / TextureEndpoint planned
          |
          +---- D3D11Helper
          |
          +---- D3D12Helper
```

`D3D11Helper` と `D3D12Helper` は互いを直接参照しません。
D3D11 と D3D12 の両方を知るのは `D3DInterop` のみです。

### P1 の同期モデル

Producer は共有対象への GPU 書き込みを積んだ後に `Signal(value)` します。
Consumer は共有対象を読む前に `GpuWait(value)` します。

```text
Producer queue/context                 Consumer queue/context
----------------------                 ----------------------
write resource
Signal(fence, N)      -------------->  GpuWait(fence, N)
                                       read resource
```

Fence value は上位プロトコルが単調増加で管理します。
同じ queue / context に `GpuWait(N)` を積んだ後、その値を満たす `Signal(N)` を同じ queue / context に積むと自己デッドロックするため、`GpuWait` は別デバイスまたは別 queue / context が signal する値を待つ用途で使います。

---

## 現在のテスト

P1 では shared fence の roundtrip test を提供します。

- D3D12 fence を作成し、D3D11 側で open して CPU wait できること
- D3D11 fence を作成し、D3D12 側で open して CPU wait できること
- D3D11 / D3D12 の fence endpoint 経由で値を往復できること

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
    D3D11Endpoint.hpp
    D3D12Endpoint.hpp
  src/
    SharedFence.cpp
    D3D11Endpoint.cpp
    D3D12Endpoint.cpp
  test/
    CMakeLists.txt
    test_fence_roundtrip.cpp
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

| Phase | 内容 |
| --- | --- |
| P0 | `D3D12Fence` shared handle 対応 |
| P1 | `SharedFence` / D3D11・D3D12 fence endpoint / fence roundtrip test |
| P2 | `SharedTexture` / texture endpoint / 12→11 texture handoff |
| P3 | 4 象限展開 |
| P4 | ping-pong / feedback loop sample |

---

## ライセンス

未定です。GitHub 公開時には、必要に応じて `LICENSE` を追加してください。
