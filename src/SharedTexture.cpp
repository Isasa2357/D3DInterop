//
// SharedTexture.cpp
//
#include "SharedTexture.hpp"

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Core/D3D11SharedResource.hpp>
#include <D3D11Helper/D3D11Framework/D3D11Helpers.hpp>
#include <D3D11Helper/D3D11Framework/D3D11Resource.hpp>

#include <D3D12Helper/D3D12Core/D3D12Core.hpp>
#include <D3D12Helper/D3D12Core/D3D12SharedResource.hpp>
#include <D3D12Helper/D3D12Framework/D3D12Helpers.hpp>
#include <D3D12Helper/D3D12Framework/D3D12Resource.hpp>

#include <stdexcept>
#include <utility>

namespace D3DInteropLib {

namespace {

D3D12_RESOURCE_FLAGS MakeD3D12ResourceFlags(const SharedTextureDesc& desc) {
    // D3D12-created textures that are opened by D3D11 must be shareable not only
    // at the NT-handle/heap level but also at the resource-access level.
    // Without ALLOW_SIMULTANEOUS_ACCESS, ID3D11Device1::OpenSharedResource1 can
    // fail with E_INVALIDARG on the D3D12 -> D3D11 texture quadrant.
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
    if (desc.allowRenderTarget) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (desc.allowUnorderedAccess) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    return flags;
}

UINT MakeD3D11BindFlags(const SharedTextureDesc& desc) {
    UINT flags = D3D11_BIND_SHADER_RESOURCE;
    if (desc.allowRenderTarget) {
        flags |= D3D11_BIND_RENDER_TARGET;
    }
    if (desc.allowUnorderedAccess) {
        flags |= D3D11_BIND_UNORDERED_ACCESS;
    }
    return flags;
}

D3D11CoreLib::D3D11SharedTextureSyncMode ToD3D11SyncMode(SyncPolicy sync) {
    switch (sync) {
    case SyncPolicy::SharedFence:
        return D3D11CoreLib::D3D11SharedTextureSyncMode::SharedFence;
    case SyncPolicy::KeyedMutex:
        return D3D11CoreLib::D3D11SharedTextureSyncMode::KeyedMutex;
    default:
        throw std::runtime_error("SharedTexture: unknown SyncPolicy");
    }
}

} // namespace

SharedTexture::SharedTexture(HANDLE handle,
                             LUID adapterLuid,
                             OwnerApi owner,
                             const SharedTextureDesc& desc,
                             ComPtr<ID3D12Resource> owner12Resource,
                             ComPtr<ID3D11Texture2D> owner11Texture) noexcept
    : m_handle(handle)
    , m_adapterLuid(adapterLuid)
    , m_owner(owner)
    , m_desc(desc)
    , m_owner12Resource(std::move(owner12Resource))
    , m_owner11Texture(std::move(owner11Texture)) {}

SharedTexture::~SharedTexture() {
    Destroy();
}

void SharedTexture::Destroy() noexcept {
    if (m_handle && m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
        m_handle = nullptr;
    }
    m_owner12Resource.Reset();
    m_owner11Texture.Reset();
    m_adapterLuid = {};
    m_owner = OwnerApi::D3D12;
    m_desc = {};
}

SharedTexture::SharedTexture(SharedTexture&& other) noexcept
    : m_handle(other.m_handle)
    , m_adapterLuid(other.m_adapterLuid)
    , m_owner(other.m_owner)
    , m_desc(other.m_desc)
    , m_owner12Resource(std::move(other.m_owner12Resource))
    , m_owner11Texture(std::move(other.m_owner11Texture)) {
    other.m_handle = nullptr;
    other.m_adapterLuid = {};
    other.m_owner = OwnerApi::D3D12;
    other.m_desc = {};
}

SharedTexture& SharedTexture::operator=(SharedTexture&& other) noexcept {
    if (this != &other) {
        Destroy();
        m_handle = other.m_handle;
        m_adapterLuid = other.m_adapterLuid;
        m_owner = other.m_owner;
        m_desc = other.m_desc;
        m_owner12Resource = std::move(other.m_owner12Resource);
        m_owner11Texture = std::move(other.m_owner11Texture);

        other.m_handle = nullptr;
        other.m_adapterLuid = {};
        other.m_owner = OwnerApi::D3D12;
        other.m_desc = {};
    }
    return *this;
}

SharedTexture SharedTexture::CreateOnD3D12(D3D12CoreLib::D3D12Core& core,
                                           const SharedTextureDesc& desc) {
    ValidateSharedTextureDesc(desc, "SharedTexture::CreateOnD3D12");
    if (desc.sync == SyncPolicy::KeyedMutex) {
        throw std::runtime_error(
            "SharedTexture::CreateOnD3D12: KeyedMutex is D3D11-only in this version");
    }

    D3D12CoreLib::D3D12Resource resource = D3D12CoreLib::CreateSharedTexture2D(
        core,
        desc.width,
        desc.height,
        desc.format,
        D3D12_RESOURCE_STATE_COMMON,
        MakeD3D12ResourceFlags(desc),
        desc.arraySize,
        desc.mipLevels);

    HANDLE handle = D3D12CoreLib::D3D12SharedResource::CreateSharedHandle(
        core.GetDevice(), resource.Get());

    ComPtr<ID3D12Resource> keepAlive = resource.Get();
    return SharedTexture(handle, core.GetAdapterLuid(), OwnerApi::D3D12, desc, keepAlive, nullptr);
}

SharedTexture SharedTexture::CreateOnD3D11(D3D11CoreLib::D3D11Core& core,
                                           const SharedTextureDesc& desc) {
    ValidateSharedTextureDesc(desc, "SharedTexture::CreateOnD3D11");

    D3D11CoreLib::D3D11Resource resource = D3D11CoreLib::CreateSharedTexture2D(
        core,
        desc.width,
        desc.height,
        desc.format,
        MakeD3D11BindFlags(desc),
        ToD3D11SyncMode(desc.sync));

    HANDLE handle = D3D11CoreLib::D3D11SharedResource::CreateSharedHandle(resource.Get());

    ComPtr<ID3D11Texture2D> keepAlive = resource.AsTexture2D();
    return SharedTexture(handle, core.GetAdapterLuid(), OwnerApi::D3D11, desc, nullptr, keepAlive);
}

} // namespace D3DInteropLib
