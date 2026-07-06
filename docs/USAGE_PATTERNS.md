# D3DInterop Usage Patterns

This document summarizes the intended v0.1 usage patterns.

## 1. Main cross-API path: D3D11 allocator -> D3D12 opener

The stable cross-API texture path is:

```text
D3D11 creates a shared Texture2D
D3D12 opens the NT handle
D3D11 and D3D12 synchronize through SharedFence
```

Use this path even when the final operation is conceptually `D3D12 -> D3D11`.
The key rule is that the texture object should still be allocated by D3D11.
D3D12 may open it and write into it through RTV/UAV views, then D3D11 can read,
render, encode, or present it after the shared fence is signaled.

```text
Input texture:
  D3D11 allocate -> D3D11 write -> D3D12 open/read

Output texture:
  D3D11 allocate -> D3D12 open/write -> D3D11 read/render/encode
```

This avoids the unsupported `D3D12 allocator -> D3D11 opener` quadrant.

## 2. Single texture handoff

For a one-resource producer/consumer handoff, use:

```cpp
D3DInteropLib::D3D11ToD3D12TextureChannel
```

Typical flow:

```text
BeginProduce()
D3D11 writes producer texture
EndProduce(D3D11 context, fenceValue)
D3D12 waits with WaitReadyOnConsumerGpu(...)
D3D12 reads consumer texture
EndConsume(D3D12 queue, fenceValue)
WaitConsumedOnProducerCpu(fenceValue)
```

This is useful for simple pipelines, validation samples, and low-frequency
handoffs.

## 3. Multi-slot real-time handoff

For real-time camera/video style pipelines, prefer:

```cpp
D3DInteropLib::D3D11ToD3D12TextureRingChannel
```

The ring channel avoids stalling on a single texture slot. The producer can write
to one slot while the consumer is still working on a previous slot.

Use this for:

```text
camera frame -> D3D12 compute
video decode frame -> D3D12 processing
D3D11 renderer -> D3D12 ML/processing
```

## 4. D3D12 processing result back to D3D11

Do not allocate the output texture on D3D12 if D3D11 must later open it.
Instead:

```text
1. D3D11 creates the output shared texture with allowUnorderedAccess=true or allowRenderTarget=true.
2. D3D12 opens the texture.
3. D3D12 writes to it using UAV or RTV.
4. D3D12 signals a shared fence.
5. D3D11 waits before using the output texture.
```

The P13 sample `sample_end_to_end_11_12_11` demonstrates this pattern.

## 5. D3D11 / D3D11 KeyedMutex path

For D3D11-only sharing between two D3D11 devices, use:

```cpp
D3DInteropLib::D3D11ToD3D11KeyedMutexChannel
D3DInteropLib::D3D11ToD3D11KeyedMutexRingChannel
```

The fixed per-slot protocol is:

```text
D3D11 producer: Acquire(0) -> write -> Release(1)
D3D11 consumer: Acquire(1) -> read  -> Release(0)
```

## 6. D3D12 view helper policy

D3DInterop creates descriptors into application-provided descriptor heap slots.
It does not own descriptor heaps.

Available helper families:

```cpp
CreateD3D12Texture2DSrv(...)
CreateD3D12Texture2DRtv(...)
CreateD3D12Texture2DUav(...)
```

The application remains responsible for descriptor heap allocation, descriptor
lifetime, shader-visible heap binding, and resource state transitions.

## 7. Video formats

For multi-plane video formats, create plane-specific SRVs:

```text
NV12:
  Plane 0: DXGI_FORMAT_R8_UNORM
  Plane 1: DXGI_FORMAT_R8G8_UNORM

P010 / P016:
  Plane 0: DXGI_FORMAT_R16_UNORM
  Plane 1: DXGI_FORMAT_R16G16_UNORM

YUY2:
  SRV format: DXGI_FORMAT_YUY2
```

The P12 sample `sample_nv12_to_rgba_compute_11_to_12` demonstrates NV12 plane
SRVs feeding a D3D12 compute shader.
