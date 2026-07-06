//
// D3D12TextureViewHelpers.cpp
//
#include "D3D12TextureViewHelpers.hpp"

#include <stdexcept>
#include <string>

namespace D3DInteropLib {

namespace {

bool IsTypelessFormat(DXGI_FORMAT format) noexcept {
    switch (format) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC7_TYPELESS:
        return true;
    default:
        return false;
    }
}

bool IsPlanarVideoFormat(DXGI_FORMAT format) noexcept {
    switch (format) {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
        return true;
    default:
        return false;
    }
}

void ThrowIfInvalidEndpoint(const D3D12TextureEndpoint& endpoint, const char* apiName) {
    if (!endpoint.Get()) {
        throw std::runtime_error(std::string(apiName) + ": endpoint has no D3D12 resource");
    }

    const D3D12_RESOURCE_DESC resourceDesc = endpoint.Get()->GetDesc();
    if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
        throw std::runtime_error(std::string(apiName) + ": endpoint resource is not Texture2D");
    }
}

void ValidateMipSlice(const SharedTextureDesc& desc, UINT mipSlice, const char* apiName) {
    if (mipSlice >= desc.mipLevels) {
        throw std::runtime_error(std::string(apiName) + ": mipSlice is out of range");
    }
}

void ValidatePlaneSlice(const SharedTextureDesc& desc, UINT planeSlice, const char* apiName) {
    const auto formatSet = GetD3DInteropTextureViewFormatSet(desc.format);
    if (planeSlice >= formatSet.planeCount) {
        throw std::runtime_error(std::string(apiName) + ": planeSlice is out of range");
    }
}

UINT ResolveMipLevels(const SharedTextureDesc& desc,
                      UINT mostDetailedMip,
                      UINT requestedMipLevels,
                      const char* apiName) {
    if (mostDetailedMip >= desc.mipLevels) {
        throw std::runtime_error(std::string(apiName) + ": mostDetailedMip is out of range");
    }

    const UINT remaining = static_cast<UINT>(desc.mipLevels) - mostDetailedMip;
    if (requestedMipLevels == D3DInteropAllTextureMips) {
        return remaining;
    }
    if (requestedMipLevels == 0 || requestedMipLevels > remaining) {
        throw std::runtime_error(std::string(apiName) + ": mipLevels is out of range");
    }
    return requestedMipLevels;
}

DXGI_FORMAT SelectDefaultFormat(const D3DInteropTextureViewFormatSet& formatSet,
                                D3D12TextureViewUsage usage) noexcept {
    switch (usage) {
    case D3D12TextureViewUsage::ShaderResource:
        return formatSet.defaultSrvFormat;
    case D3D12TextureViewUsage::RenderTarget:
        return formatSet.defaultRtvFormat;
    case D3D12TextureViewUsage::UnorderedAccess:
        return formatSet.defaultUavFormat;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

const char* UsageName(D3D12TextureViewUsage usage) noexcept {
    switch (usage) {
    case D3D12TextureViewUsage::ShaderResource:
        return "SRV";
    case D3D12TextureViewUsage::RenderTarget:
        return "RTV";
    case D3D12TextureViewUsage::UnorderedAccess:
        return "UAV";
    default:
        return "view";
    }
}

} // namespace

D3DInteropTextureViewFormatSet GetD3DInteropTextureViewFormatSet(DXGI_FORMAT resourceFormat) {
    D3DInteropTextureViewFormatSet set;
    set.resourceFormat = resourceFormat;
    set.defaultSrvFormat = resourceFormat;
    set.defaultRtvFormat = resourceFormat;
    set.defaultUavFormat = resourceFormat;
    set.planeCount = 1;
    set.planeSrvFormats[0] = resourceFormat;
    set.planeSrvFormats[1] = DXGI_FORMAT_UNKNOWN;

    switch (resourceFormat) {
    case DXGI_FORMAT_UNKNOWN:
        set.defaultSrvFormat = DXGI_FORMAT_UNKNOWN;
        set.defaultRtvFormat = DXGI_FORMAT_UNKNOWN;
        set.defaultUavFormat = DXGI_FORMAT_UNKNOWN;
        set.planeSrvFormats[0] = DXGI_FORMAT_UNKNOWN;
        break;

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        set.defaultSrvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        set.defaultRtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        set.defaultUavFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        set.planeSrvFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;

    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        set.defaultSrvFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        set.defaultRtvFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        set.defaultUavFormat = DXGI_FORMAT_UNKNOWN;
        set.planeSrvFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
        break;

    case DXGI_FORMAT_R32_TYPELESS:
        set.defaultSrvFormat = DXGI_FORMAT_R32_FLOAT;
        set.defaultRtvFormat = DXGI_FORMAT_R32_FLOAT;
        set.defaultUavFormat = DXGI_FORMAT_R32_FLOAT;
        set.planeSrvFormats[0] = DXGI_FORMAT_R32_FLOAT;
        break;

    case DXGI_FORMAT_R16_TYPELESS:
        set.defaultSrvFormat = DXGI_FORMAT_R16_UNORM;
        set.defaultRtvFormat = DXGI_FORMAT_R16_UNORM;
        set.defaultUavFormat = DXGI_FORMAT_R16_UNORM;
        set.planeSrvFormats[0] = DXGI_FORMAT_R16_UNORM;
        break;

    case DXGI_FORMAT_R8_TYPELESS:
        set.defaultSrvFormat = DXGI_FORMAT_R8_UNORM;
        set.defaultRtvFormat = DXGI_FORMAT_R8_UNORM;
        set.defaultUavFormat = DXGI_FORMAT_R8_UNORM;
        set.planeSrvFormats[0] = DXGI_FORMAT_R8_UNORM;
        break;

    case DXGI_FORMAT_NV12:
        set.defaultSrvFormat = DXGI_FORMAT_UNKNOWN;
        set.defaultRtvFormat = DXGI_FORMAT_UNKNOWN;
        set.defaultUavFormat = DXGI_FORMAT_UNKNOWN;
        set.planeCount = 2;
        set.planeSrvFormats[0] = DXGI_FORMAT_R8_UNORM;
        set.planeSrvFormats[1] = DXGI_FORMAT_R8G8_UNORM;
        break;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
        set.defaultSrvFormat = DXGI_FORMAT_UNKNOWN;
        set.defaultRtvFormat = DXGI_FORMAT_UNKNOWN;
        set.defaultUavFormat = DXGI_FORMAT_UNKNOWN;
        set.planeCount = 2;
        set.planeSrvFormats[0] = DXGI_FORMAT_R16_UNORM;
        set.planeSrvFormats[1] = DXGI_FORMAT_R16G16_UNORM;
        break;

    case DXGI_FORMAT_YUY2:
        set.defaultSrvFormat = DXGI_FORMAT_YUY2;
        set.defaultRtvFormat = DXGI_FORMAT_UNKNOWN;
        set.defaultUavFormat = DXGI_FORMAT_UNKNOWN;
        set.planeCount = 1;
        set.planeSrvFormats[0] = DXGI_FORMAT_YUY2;
        break;

    default:
        break;
    }

    return set;
}

DXGI_FORMAT GetD3DInteropVideoPlaneSrvFormat(DXGI_FORMAT resourceFormat, UINT planeSlice) {
    const auto formatSet = GetD3DInteropTextureViewFormatSet(resourceFormat);
    if (planeSlice >= formatSet.planeCount) {
        throw std::runtime_error("GetD3DInteropVideoPlaneSrvFormat: planeSlice is out of range");
    }

    const DXGI_FORMAT format = formatSet.planeSrvFormats[planeSlice];
    if (format == DXGI_FORMAT_UNKNOWN) {
        throw std::runtime_error("GetD3DInteropVideoPlaneSrvFormat: no plane SRV format is defined for this resource format");
    }
    return format;
}

DXGI_FORMAT ResolveD3D12TextureViewFormat(
    const D3D12TextureEndpoint& endpoint,
    DXGI_FORMAT requestedFormat,
    D3D12TextureViewUsage usage) {

    if (requestedFormat != DXGI_FORMAT_UNKNOWN) {
        return requestedFormat;
    }

    const DXGI_FORMAT resourceFormat = endpoint.Desc().format;
    if (resourceFormat == DXGI_FORMAT_UNKNOWN) {
        throw std::runtime_error("ResolveD3D12TextureViewFormat: no valid resource format is available");
    }

    const auto formatSet = GetD3DInteropTextureViewFormatSet(resourceFormat);
    const DXGI_FORMAT defaultFormat = SelectDefaultFormat(formatSet, usage);
    if (defaultFormat != DXGI_FORMAT_UNKNOWN) {
        return defaultFormat;
    }

    if (IsPlanarVideoFormat(resourceFormat)) {
        throw std::runtime_error(
            std::string("ResolveD3D12TextureViewFormat: ") + UsageName(usage) +
            " format must be specified explicitly for planar video formats");
    }

    if (IsTypelessFormat(resourceFormat)) {
        throw std::runtime_error(
            std::string("ResolveD3D12TextureViewFormat: no default typed ") + UsageName(usage) +
            " format is registered for this typeless resource format");
    }

    return resourceFormat;
}

D3D12_SHADER_RESOURCE_VIEW_DESC MakeD3D12Texture2DSrvDesc(
    const D3D12TextureEndpoint& endpoint,
    const D3D12Texture2DSrvOptions& options) {

    constexpr const char* kApiName = "MakeD3D12Texture2DSrvDesc";
    ThrowIfInvalidEndpoint(endpoint, kApiName);
    ValidatePlaneSlice(endpoint.Desc(), options.planeSlice, kApiName);

    D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format = ResolveD3D12TextureViewFormat(endpoint, options.format, D3D12TextureViewUsage::ShaderResource);
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Texture2D.MostDetailedMip = options.mostDetailedMip;
    desc.Texture2D.MipLevels = ResolveMipLevels(
        endpoint.Desc(),
        options.mostDetailedMip,
        options.mipLevels,
        kApiName);
    desc.Texture2D.PlaneSlice = options.planeSlice;
    desc.Texture2D.ResourceMinLODClamp = options.resourceMinLODClamp;
    return desc;
}

void CreateD3D12Texture2DSrv(
    ID3D12Device* device,
    const D3D12TextureEndpoint& endpoint,
    D3D12_CPU_DESCRIPTOR_HANDLE destination,
    const D3D12Texture2DSrvOptions& options) {

    if (!device) {
        throw std::runtime_error("CreateD3D12Texture2DSrv: null ID3D12Device");
    }

    const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = MakeD3D12Texture2DSrvDesc(endpoint, options);
    device->CreateShaderResourceView(endpoint.Get(), &srvDesc, destination);
}

D3D12_RENDER_TARGET_VIEW_DESC MakeD3D12Texture2DRtvDesc(
    const D3D12TextureEndpoint& endpoint,
    const D3D12Texture2DRtvOptions& options) {

    constexpr const char* kApiName = "MakeD3D12Texture2DRtvDesc";
    ThrowIfInvalidEndpoint(endpoint, kApiName);
    ValidateMipSlice(endpoint.Desc(), options.mipSlice, kApiName);
    ValidatePlaneSlice(endpoint.Desc(), options.planeSlice, kApiName);

    D3D12_RENDER_TARGET_VIEW_DESC desc = {};
    desc.Format = ResolveD3D12TextureViewFormat(endpoint, options.format, D3D12TextureViewUsage::RenderTarget);
    desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipSlice = options.mipSlice;
    desc.Texture2D.PlaneSlice = options.planeSlice;
    return desc;
}

void CreateD3D12Texture2DRtv(
    ID3D12Device* device,
    const D3D12TextureEndpoint& endpoint,
    D3D12_CPU_DESCRIPTOR_HANDLE destination,
    const D3D12Texture2DRtvOptions& options) {

    if (!device) {
        throw std::runtime_error("CreateD3D12Texture2DRtv: null ID3D12Device");
    }

    const D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = MakeD3D12Texture2DRtvDesc(endpoint, options);
    device->CreateRenderTargetView(endpoint.Get(), &rtvDesc, destination);
}

D3D12_UNORDERED_ACCESS_VIEW_DESC MakeD3D12Texture2DUavDesc(
    const D3D12TextureEndpoint& endpoint,
    const D3D12Texture2DUavOptions& options) {

    constexpr const char* kApiName = "MakeD3D12Texture2DUavDesc";
    ThrowIfInvalidEndpoint(endpoint, kApiName);
    ValidateMipSlice(endpoint.Desc(), options.mipSlice, kApiName);
    ValidatePlaneSlice(endpoint.Desc(), options.planeSlice, kApiName);

    D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
    desc.Format = ResolveD3D12TextureViewFormat(endpoint, options.format, D3D12TextureViewUsage::UnorderedAccess);
    desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipSlice = options.mipSlice;
    desc.Texture2D.PlaneSlice = options.planeSlice;
    return desc;
}

void CreateD3D12Texture2DUav(
    ID3D12Device* device,
    const D3D12TextureEndpoint& endpoint,
    D3D12_CPU_DESCRIPTOR_HANDLE destination,
    const D3D12Texture2DUavOptions& options) {

    if (!device) {
        throw std::runtime_error("CreateD3D12Texture2DUav: null ID3D12Device");
    }

    const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = MakeD3D12Texture2DUavDesc(endpoint, options);
    device->CreateUnorderedAccessView(endpoint.Get(), nullptr, &uavDesc, destination);
}

} // namespace D3DInteropLib
