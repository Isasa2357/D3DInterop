#pragma once
//
// SharedTexture.hpp
// Cross-API shared Texture2D NT handle owner.
// P2 implements D3D12 allocator -> D3D11 opener as the first texture quadrant.
//
#include "D3DInteropCommon.hpp"

namespace D3D11CoreLib { class D3D11Core; }
namespace D3D12CoreLib { class D3D12Core; }

namespace D3DInteropLib {

class SharedTexture {
public:
    SharedTexture() = default;
    ~SharedTexture();

    static SharedTexture CreateOnD3D12(D3D12CoreLib::D3D12Core& core,
                                       const SharedTextureDesc& desc);
    static SharedTexture CreateOnD3D11(D3D11CoreLib::D3D11Core& core,
                                       const SharedTextureDesc& desc);

    SharedTexture(SharedTexture&& other) noexcept;
    SharedTexture& operator=(SharedTexture&& other) noexcept;

    SharedTexture(const SharedTexture&)            = delete;
    SharedTexture& operator=(const SharedTexture&) = delete;

    HANDLE                   TextureHandle() const noexcept { return m_handle; }
    const SharedTextureDesc& Desc()          const noexcept { return m_desc; }
    LUID                     AdapterLuid()   const noexcept { return m_adapterLuid; }
    OwnerApi                 Owner()         const noexcept { return m_owner; }
    bool                     IsValid()       const noexcept {
        return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE;
    }

private:
    SharedTexture(HANDLE handle,
                  LUID adapterLuid,
                  OwnerApi owner,
                  const SharedTextureDesc& desc,
                  ComPtr<ID3D12Resource> owner12Resource = {},
                  ComPtr<ID3D11Texture2D> owner11Texture = {}) noexcept;

    void Destroy() noexcept;

    HANDLE m_handle = nullptr;
    LUID   m_adapterLuid{};
    OwnerApi m_owner = OwnerApi::D3D12;
    SharedTextureDesc m_desc{};

    // Keep the allocator-side object alive in addition to the NT handle.
    // Endpoints still open through the NT handle so allocator/opener code paths stay symmetric.
    ComPtr<ID3D12Resource>  m_owner12Resource;
    ComPtr<ID3D11Texture2D> m_owner11Texture;
};

} // namespace D3DInteropLib
