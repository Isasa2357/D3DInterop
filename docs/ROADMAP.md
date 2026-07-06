# D3DInterop Roadmap

## v0.1 target

v0.1 is the release-hardening boundary for D3DInterop.

Included:

```text
D3D11 -> D3D12 shared texture/fence path
D3D11 -> D3D12 channel and ring channel
D3D11 -> D3D11 KeyedMutex channel and ring channel
D3D12 SRV / RTV / UAV helpers
NV12 / P010 / YUY2 view policy
NV12 -> RGBA compute sample
D3D11 -> D3D12 -> D3D11 end-to-end sample
install/package/external consumer test
```

## Recommended next layer

After v0.1, new functionality should generally move to upper layers rather than
expanding D3DInterop indefinitely.

Recommended next projects:

```text
D3DProcessing
  - NV12 -> RGBA
  - resize / crop
  - format conversion
  - shader library
  - fused HLSL processing path

D3DVideoEncoder
  - D3D11 texture input
  - H.264 / H.265 encode
  - Media Foundation backend

MFD3DCapture
  - camera/video input
  - D3D11 texture output
```

## Possible v0.2 items

Only add these when a concrete user exists:

```text
D3D12 -> D3D12 high-level channel
cross-process handle protocol
cross-adapter experiments
MSAA / depth-stencil validation
more video formats and color spaces
optional descriptor heap allocator
```

## Keep unsupported unless proven otherwise

The `D3D12 allocator -> D3D11 opener` route should remain unsupported unless a
reliable implementation strategy is validated across target machines and drivers.
