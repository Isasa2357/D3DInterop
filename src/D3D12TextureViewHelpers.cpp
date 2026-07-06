//
// D3D12TextureViewHelpers.cpp
//
#include "D3D12TextureViewHelpers.hpp"

#include <stdexcept>
#include <string>

namespace D3DInteropLib {

namespace {

void ThrowIfInvalidEndpoint(const D3D12TextureEndpoint& endpoint, const char* apiName) {
    if (!endpoint.Get()) {
        throw std::runtime_error(std::string(apiName) + ": endpoint has no D3D12 resource");
    }

    const D3D12_RESOURCE_DESC resourceDesc = endpoint.Get()->GetDesc();
    if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
        throw std::runtime_error(std::string(apiName) + ": endpoint resource is not Texture2D");
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

} // namespace

DXGI_FORMAT ResolveD3D12TextureViewFormat(const D3D12TextureEndpoint& endpoint,
                                          DXGI_FORMAT requestedFormat) {
    if (requestedFormat != DXGI_FORMAT_UNKNOWN) {
        return requestedFormat;
    }

    const DXGI_FORMAT format = endpoint.Desc().format;
    if (format == DXGI_FORMAT_UNKNOWN) {
        throw std::runtime_error("ResolveD3D12TextureViewFormat: no valid format is available");
    }
    return format;
}

D3D12_SHADER_RESOURCE_VIEW_DESC MakeD3D12Texture2DSrvDesc(
    const D3D12TextureEndpoint& endpoint,
    const D3D12Texture2DSrvOptions& options) {

    constexpr const char* kApiName = "MakeD3D12Texture2DSrvDesc";
    ThrowIfInvalidEndpoint(endpoint, kApiName);

    D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format = ResolveD3D12TextureViewFormat(endpoint, options.format);
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

} // namespace D3DInteropLib
