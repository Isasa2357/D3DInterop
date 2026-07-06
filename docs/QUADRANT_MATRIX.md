# D3DInterop quadrant matrix

This document defines the supported, validated, optional, and unsupported texture-sharing quadrants for `D3DInterop`.

`D3DInterop` currently treats **D3D11 allocator -> D3D12 opener** as the validated texture-sharing path. Other quadrants are intentionally not exposed as high-level channels until they are validated on real hardware.

## Status table

| Allocator / owner | Opener / user | Texture handle | Synchronization | Status |
| --- | --- | --- | --- | --- |
| D3D11 | D3D12 | NT shared handle | Shared fence | **Supported / validated** |
| D3D11 | D3D11 | NT shared handle | Keyed mutex | **Low-level supported / minimally tested** |
| D3D12 | D3D12 | NT shared handle | Shared fence | Low-level possible, high-level channel not provided |
| D3D12 | D3D11 | NT shared handle | Shared fence | **Unsupported in this library** |

## Validated path

```text
D3D11 allocator / producer -> D3D12 opener / consumer
```

This is the path used by the high-level APIs:

```text
D3D11ToD3D12TextureChannel
D3D11ToD3D12TextureRingChannel
```

The texture is allocated by D3D11, opened by D3D12, and synchronized with shared fences.

## Unsupported path: D3D12 allocator -> D3D11 opener

The following path is not currently supported by `D3DInterop`:

```text
D3D12 allocator / producer -> D3D11 opener / consumer
```

In other words, **a shared Texture2D created by D3D12 is not currently supported as a D3D11 texture in this library**.

On the current validation environment, attempting to open a D3D12-created shared texture from D3D11 with `ID3D11Device1::OpenSharedResource1` returned `E_INVALIDARG`. Because this behavior can depend on driver, resource flags, format, and resource creation details, `D3DInterop` treats this quadrant as unsupported rather than exposing an unreliable high-level API.

Do not build application code that assumes a D3D12-owned texture can be opened and used from D3D11 through this library.

## D3D11 -> D3D11 keyed mutex path

The D3D11-to-D3D11 keyed mutex path is available as a low-level validation path:

```text
D3D11 allocator -> D3D11 opener
SyncPolicy::KeyedMutex
D3D11TextureEndpoint::AcquireKey / ReleaseKey
```

This path is intended for D3D11-only interop cases and does not use shared fences.

## D3D12 -> D3D12 path

D3D12-to-D3D12 shared resource opening is conceptually possible through NT shared handles and shared fences. However, `D3DInterop` does not currently provide a high-level channel API for this quadrant, because the main validated use case is D3D11 producer -> D3D12 consumer.

## Policy

High-level APIs should only be added for quadrants that are validated by tests and examples.

Unsupported quadrants should fail explicitly or remain absent from the high-level API surface rather than appearing to work only on some machines.
