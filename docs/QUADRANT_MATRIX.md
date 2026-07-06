# D3DInterop Quadrant Matrix

`D3DInterop` currently treats `D3D11 allocator -> D3D12 opener` as the validated D3D11 / D3D12 texture sharing path.

## Texture2D sharing support

| Allocator / owner | Opener / user | Status | Notes |
| --- | --- | --- | --- |
| D3D11 | D3D12 | Supported / validated | Main cross-API path. Used by `D3D11ToD3D12TextureChannel` and `D3D11ToD3D12TextureRingChannel`. |
| D3D12 | D3D11 | Unsupported | API-level rejected by `D3D11TextureEndpoint::Open()`. Do not rely on raw `OpenSharedResource1` behavior. |
| D3D11 | D3D11 | Supported / validated | KeyedMutex path. Used by `D3D11ToD3D11KeyedMutexChannel` and `D3D11ToD3D11KeyedMutexRingChannel`. |
| D3D12 | D3D12 | Low-level only | Shared handle/fence primitives exist, but no high-level texture channel API is provided yet. |

## Explicit unsupported rule

The following route is intentionally unsupported in the current version:

```text
D3D12 allocator -> D3D11 opener
```

This means a shared `Texture2D` created by D3D12 must not be opened through `D3D11TextureEndpoint::Open()` and used as a D3D11 texture.

The reason is practical: on the tested environment, `ID3D11Device1::OpenSharedResource1` returned `E_INVALIDARG` for the D3D12-owned texture path. D3DInterop therefore rejects this route before calling the raw D3D11 open API, so applications get a stable and documented error instead of a driver-dependent failure.

Use this cross-API route instead:

```text
D3D11 allocator -> D3D12 opener
```

## Current high-level APIs

### D3D11 -> D3D12 / SharedFence

- `D3D11ToD3D12TextureChannel`
- `D3D11ToD3D12TextureRingChannel`

Both are built on the validated `D3D11 allocator -> D3D12 opener` path.

### D3D11 -> D3D11 / KeyedMutex

- `D3D11ToD3D11KeyedMutexChannel`
- `D3D11ToD3D11KeyedMutexRingChannel`

Both use the following fixed keyed mutex protocol per slot:

```text
D3D11 producer: Acquire(0) -> write -> Release(1)
D3D11 consumer: Acquire(1) -> read  -> Release(0)
```

## Typed D3D12 SRV helpers

For the validated `D3D11 allocator -> D3D12 opener` path, D3DInterop provides helper functions that create typed `Texture2D` SRV descriptors for a `D3D12TextureEndpoint`.

The helper does not allocate or own descriptor heaps. Applications still decide descriptor heap lifetime and descriptor placement.

For NV12, use plane-specific typed SRVs:

```text
Plane 0: DXGI_FORMAT_R8_UNORM,   planeSlice = 0
Plane 1: DXGI_FORMAT_R8G8_UNORM, planeSlice = 1
```
