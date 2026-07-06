# Typed Texture View Policy

D3DInterop keeps texture allocation and endpoint opening separate from descriptor heap ownership. Applications still allocate descriptor heaps and choose descriptor placement. D3DInterop only creates typed descriptors into application-provided CPU descriptor handles.

## Why this exists

Shared textures often use typeless or video formats:

- `DXGI_FORMAT_R8G8B8A8_TYPELESS`
- `DXGI_FORMAT_NV12`
- `DXGI_FORMAT_P010`
- `DXGI_FORMAT_YUY2`

D3D12 views are typed. For typeless resources, the view format must be a concrete compatible typed format. For planar video formats, the view must target the correct plane with the correct plane format.

## Typeless defaults

D3DInterop provides a small default policy through `GetD3DInteropTextureViewFormatSet()`.

| Resource format | Default SRV | Default RTV | Default UAV |
| --- | --- | --- | --- |
| `DXGI_FORMAT_R8G8B8A8_TYPELESS` | `DXGI_FORMAT_R8G8B8A8_UNORM` | `DXGI_FORMAT_R8G8B8A8_UNORM` | `DXGI_FORMAT_R8G8B8A8_UNORM` |
| `DXGI_FORMAT_B8G8R8A8_TYPELESS` | `DXGI_FORMAT_B8G8R8A8_UNORM` | `DXGI_FORMAT_B8G8R8A8_UNORM` | `DXGI_FORMAT_UNKNOWN` |
| `DXGI_FORMAT_R32_TYPELESS` | `DXGI_FORMAT_R32_FLOAT` | `DXGI_FORMAT_R32_FLOAT` | `DXGI_FORMAT_R32_FLOAT` |
| `DXGI_FORMAT_R16_TYPELESS` | `DXGI_FORMAT_R16_UNORM` | `DXGI_FORMAT_R16_UNORM` | `DXGI_FORMAT_R16_UNORM` |
| `DXGI_FORMAT_R8_TYPELESS` | `DXGI_FORMAT_R8_UNORM` | `DXGI_FORMAT_R8_UNORM` | `DXGI_FORMAT_R8_UNORM` |

If the default is not appropriate, pass an explicit format in the view options.

```cpp
D3DInteropLib::D3D12Texture2DSrvOptions options;
options.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
D3DInteropLib::CreateD3D12Texture2DSrv(device, endpoint, handle, options);
```

## Video plane SRV formats

Use `GetD3DInteropVideoPlaneSrvFormat(resourceFormat, planeSlice)` for supported video formats.

| Resource format | Plane | SRV format |
| --- | --- | --- |
| `DXGI_FORMAT_NV12` | 0 | `DXGI_FORMAT_R8_UNORM` |
| `DXGI_FORMAT_NV12` | 1 | `DXGI_FORMAT_R8G8_UNORM` |
| `DXGI_FORMAT_P010` | 0 | `DXGI_FORMAT_R16_UNORM` |
| `DXGI_FORMAT_P010` | 1 | `DXGI_FORMAT_R16G16_UNORM` |
| `DXGI_FORMAT_P016` | 0 | `DXGI_FORMAT_R16_UNORM` |
| `DXGI_FORMAT_P016` | 1 | `DXGI_FORMAT_R16G16_UNORM` |
| `DXGI_FORMAT_YUY2` | 0 | `DXGI_FORMAT_YUY2` |

Example:

```cpp
D3DInteropLib::D3D12Texture2DSrvOptions y;
y.format = D3DInteropLib::GetD3DInteropVideoPlaneSrvFormat(DXGI_FORMAT_NV12, 0);
y.planeSlice = 0;
D3DInteropLib::CreateD3D12Texture2DSrv(device, endpoint, yHandle, y);

D3DInteropLib::D3D12Texture2DSrvOptions uv;
uv.format = D3DInteropLib::GetD3DInteropVideoPlaneSrvFormat(DXGI_FORMAT_NV12, 1);
uv.planeSlice = 1;
D3DInteropLib::CreateD3D12Texture2DSrv(device, endpoint, uvHandle, uv);
```

## Descriptor heap ownership

D3DInterop deliberately does not own descriptor heaps. This keeps it usable inside engines and larger frameworks that already have descriptor allocation policies.

D3DInterop helpers take this form:

```cpp
CreateD3D12Texture2DSrv(device, endpoint, cpuDescriptorHandle, options);
CreateD3D12Texture2DRtv(device, endpoint, cpuDescriptorHandle, options);
CreateD3D12Texture2DUav(device, endpoint, cpuDescriptorHandle, options);
```

The caller is responsible for:

- descriptor heap lifetime
- descriptor allocation / recycling
- shader-visible heap binding
- resource state transitions
