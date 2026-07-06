//
// D3D12TextureEndpoint.cpp
//
#include "D3D12Endpoint.hpp"

#include <D3D12Helper/D3D12Core/D3D12SharedResource.hpp>

#include <stdexcept>

namespace D3DInteropLib {

D3D12TextureEndpoint D3D12TextureEndpoint::Open(D3D12CoreLib::D3D12Core& core,
                                                const SharedTexture& shared) {
    ThrowIfNullHandle(shared.TextureHandle(), "D3D12TextureEndpoint::Open");
    ThrowIfAdapterMismatch(shared.AdapterLuid(), core.GetAdapterLuid(), "D3D12TextureEndpoint::Open");
    if (shared.Desc().sync == SyncPolicy::KeyedMutex) {
        throw std::runtime_error(
            "D3D12TextureEndpoint::Open: KeyedMutex synchronized textures are D3D11-only in this version");
    }

    D3D12TextureEndpoint endpoint;
    endpoint.m_resource = D3D12CoreLib::D3D12SharedResource::OpenSharedTexture2D(
        core.GetDevice(), shared.TextureHandle());
    endpoint.m_desc = shared.Desc();
    endpoint.m_adapterLuid = core.GetAdapterLuid();
    return endpoint;
}

} // namespace D3DInteropLib
