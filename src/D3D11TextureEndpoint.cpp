//
// D3D11TextureEndpoint.cpp
//
#include "D3D11Endpoint.hpp"

#include <D3D11Helper/D3D11Core/D3D11SharedResource.hpp>
#include <D3D11Helper/D3D11Core/ThrowIfFailed.hpp>

#include <stdexcept>

namespace D3DInteropLib {

D3D11TextureEndpoint D3D11TextureEndpoint::Open(D3D11CoreLib::D3D11Core& core,
                                                const SharedTexture& shared) {
    ThrowIfNullHandle(shared.TextureHandle(), "D3D11TextureEndpoint::Open");
    ThrowIfAdapterMismatch(shared.AdapterLuid(), core.GetAdapterLuid(), "D3D11TextureEndpoint::Open");

    // Explicit API-level policy:
    // D3D12 allocator -> D3D11 opener is currently unsupported in this library.
    // Some drivers return E_INVALIDARG from ID3D11Device1::OpenSharedResource1 even
    // for D3D12 resources created with shared handles. Do not fall through to the
    // raw D3D11 open call; fail early with a stable, documented error instead.
    if (shared.Owner() == OwnerApi::D3D12) {
        throw std::runtime_error(
            "D3D11TextureEndpoint::Open: unsupported texture quadrant: "
            "D3D12 allocator -> D3D11 opener. "
            "D3D12-created shared Texture2D cannot currently be opened and used "
            "as a D3D11 texture by D3DInterop. Use D3D11 allocator -> D3D12 opener instead.");
    }

    D3D11TextureEndpoint endpoint;
    endpoint.m_texture = D3D11CoreLib::D3D11SharedResource::OpenSharedTexture2D(
        core.GetDevice(), shared.TextureHandle());
    endpoint.m_desc = shared.Desc();
    endpoint.m_adapterLuid = core.GetAdapterLuid();

    if (shared.Desc().sync == SyncPolicy::KeyedMutex) {
        D3D11CORE_THROW_IF_FAILED_MSG(
            endpoint.m_texture.As(&endpoint.m_keyedMutex),
            "D3D11TextureEndpoint::Open: shared texture does not expose IDXGIKeyedMutex");
    }

    return endpoint;
}

void D3D11TextureEndpoint::AcquireKey(UINT64 key, DWORD timeoutMs) {
    if (!m_keyedMutex) {
        throw std::runtime_error("D3D11TextureEndpoint::AcquireKey: texture is not KeyedMutex-synchronized");
    }
    D3D11CORE_THROW_IF_FAILED(m_keyedMutex->AcquireSync(key, timeoutMs));
}

void D3D11TextureEndpoint::ReleaseKey(UINT64 key) {
    if (!m_keyedMutex) {
        throw std::runtime_error("D3D11TextureEndpoint::ReleaseKey: texture is not KeyedMutex-synchronized");
    }
    D3D11CORE_THROW_IF_FAILED(m_keyedMutex->ReleaseSync(key));
}

} // namespace D3DInteropLib
