# D3DInterop v0.1 Limitations

This document lists intentional v0.1 limitations.

## Unsupported: D3D12 allocator -> D3D11 opener

The following route is intentionally unsupported:

```text
D3D12 creates a shared Texture2D
D3D11 opens that texture and uses it as ID3D11Texture2D
```

D3DInterop rejects this route at `D3D11TextureEndpoint::Open()` before calling
raw D3D11 open APIs. The practical reason is that tested environments returned
`E_INVALIDARG` from `ID3D11Device1::OpenSharedResource1` for this quadrant.

Use this instead:

```text
D3D11 allocate -> D3D12 open -> D3D12 write -> D3D11 use after fence
```

## Same adapter only

v0.1 validates same-adapter sharing. Adapter LUIDs are checked at endpoint open.
Cross-adapter sharing is not supported.

## Same process focused

v0.1 is designed for same-process library composition. Cross-process transport,
named handles, IPC ownership, and handle lifetime protocol are not provided.

## No descriptor heap ownership

D3DInterop provides descriptor creation helpers but does not own descriptor
heaps. Applications must manage:

```text
descriptor heap allocation
descriptor slot lifetime
shader-visible heap binding
CPU/GPU descriptor handle placement
```

## Resource states are explicit

Shared resources invalidate simple implicit state tracking assumptions. The user
must insert correct D3D12 barriers around SRV/RTV/UAV/copy use.

## No MSAA or depth/stencil validation

v0.1 focuses on common Texture2D color/video formats. MSAA and depth/stencil
sharing are not validated.

## Video processing is sample-level only

NV12/P010/YUY2 view descriptors and an NV12->RGBA compute sample exist, but a
full processing graph, resize pipeline, color management, and fused shader system
belong in a higher layer such as `D3DProcessing`.
