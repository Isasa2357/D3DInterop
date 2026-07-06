#pragma once
//
// D3D12TextureViewHelpers.hpp
// Typed D3D12 Texture2D view helpers for shared textures opened through D3DInterop.
//
// The main validated path is:
//   D3D11 allocator / producer -> D3D12 opener / consumer.
//
// These helpers intentionally create descriptors into an application-provided
// descriptor heap handle. D3DInterop does not own descriptor heaps.
//
#include "D3D12Endpoint.hpp"

namespace D3DInteropLib {

constexpr UINT D3DInteropAllTextureMips = 0xffffffffu;

enum class D3D12TextureViewUsage {
    ShaderResource,
    RenderTarget,
    UnorderedAccess
};

struct D3DInteropTextureViewFormatSet {
    DXGI_FORMAT resourceFormat = DXGI_FORMAT_UNKNOWN;

    // Default typed view formats used when a helper option keeps format UNKNOWN.
    // UNKNOWN means D3DInterop has no safe default for that view usage.
    DXGI_FORMAT defaultSrvFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT defaultRtvFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT defaultUavFormat = DXGI_FORMAT_UNKNOWN;

    // Plane-specific SRV formats for planar / video formats.
    // Non-planar formats use planeCount = 1 and planeSrvFormats[0] = defaultSrvFormat.
    UINT planeCount = 1;
    DXGI_FORMAT planeSrvFormats[2] = { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN };
};

struct D3D12Texture2DSrvOptions {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT mostDetailedMip = 0;
    UINT mipLevels = D3DInteropAllTextureMips;
    UINT planeSlice = 0;
    FLOAT resourceMinLODClamp = 0.0f;
};

struct D3D12Texture2DRtvOptions {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT mipSlice = 0;
    UINT planeSlice = 0;
};

struct D3D12Texture2DUavOptions {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT mipSlice = 0;
    UINT planeSlice = 0;
};

D3DInteropTextureViewFormatSet GetD3DInteropTextureViewFormatSet(DXGI_FORMAT resourceFormat);

DXGI_FORMAT GetD3DInteropVideoPlaneSrvFormat(DXGI_FORMAT resourceFormat,
                                             UINT planeSlice);

DXGI_FORMAT ResolveD3D12TextureViewFormat(
    const D3D12TextureEndpoint& endpoint,
    DXGI_FORMAT requestedFormat,
    D3D12TextureViewUsage usage = D3D12TextureViewUsage::ShaderResource);

D3D12_SHADER_RESOURCE_VIEW_DESC MakeD3D12Texture2DSrvDesc(
    const D3D12TextureEndpoint& endpoint,
    const D3D12Texture2DSrvOptions& options = {});

void CreateD3D12Texture2DSrv(
    ID3D12Device* device,
    const D3D12TextureEndpoint& endpoint,
    D3D12_CPU_DESCRIPTOR_HANDLE destination,
    const D3D12Texture2DSrvOptions& options = {});

D3D12_RENDER_TARGET_VIEW_DESC MakeD3D12Texture2DRtvDesc(
    const D3D12TextureEndpoint& endpoint,
    const D3D12Texture2DRtvOptions& options = {});

void CreateD3D12Texture2DRtv(
    ID3D12Device* device,
    const D3D12TextureEndpoint& endpoint,
    D3D12_CPU_DESCRIPTOR_HANDLE destination,
    const D3D12Texture2DRtvOptions& options = {});

D3D12_UNORDERED_ACCESS_VIEW_DESC MakeD3D12Texture2DUavDesc(
    const D3D12TextureEndpoint& endpoint,
    const D3D12Texture2DUavOptions& options = {});

void CreateD3D12Texture2DUav(
    ID3D12Device* device,
    const D3D12TextureEndpoint& endpoint,
    D3D12_CPU_DESCRIPTOR_HANDLE destination,
    const D3D12Texture2DUavOptions& options = {});

} // namespace D3DInteropLib
