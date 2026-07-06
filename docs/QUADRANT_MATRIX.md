# D3DInterop Quadrant Matrix

`D3DInterop` currently treats `D3D11 allocator -> D3D12 opener` as the validated texture sharing path.

## Texture2D sharing support

| Allocator / owner | Opener / user | Status | Notes |
| --- | --- | --- | --- |
| D3D11 | D3D12 | Supported / validated | Main path. Used by `D3D11ToD3D12TextureChannel` and `D3D11ToD3D12TextureRingChannel`. |
| D3D12 | D3D11 | Unsupported | API-level rejected by `D3D11TextureEndpoint::Open()`. Do not rely on raw `OpenSharedResource1` behavior. |
| D3D11 | D3D11 | Limited | KeyedMutex minimum test exists. High-level channel API is not provided yet. |
| D3D12 | D3D12 | Low-level only | Shared handle/fence primitives exist, but no high-level texture channel API is provided yet. |

## Explicit unsupported rule

The following route is intentionally unsupported in the current version:

```text
D3D12 allocator -> D3D11 opener
```

This means a shared `Texture2D` created by D3D12 must not be opened through `D3D11TextureEndpoint::Open()` and used as a D3D11 texture.

The reason is practical: on the tested environment, `ID3D11Device1::OpenSharedResource1` returned `E_INVALIDARG` for the D3D12-owned texture path. D3DInterop therefore rejects this route before calling the raw D3D11 open API, so applications get a stable and documented error instead of a driver-dependent failure.

Use this route instead:

```text
D3D11 allocator -> D3D12 opener
```

## Current high-level APIs

- `D3D11ToD3D12TextureChannel`
- `D3D11ToD3D12TextureRingChannel`

Both are built on the validated `D3D11 allocator -> D3D12 opener` path.
