//
// test_typed_srv_11_to_12.cpp
// P10: D3D11 allocator -> D3D12 opener typed SRV helper test.
//
#include <D3DInterop/D3DInterop.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D12Helper/D3D12Core/D3D12Core.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

constexpr int kSkip = 77;
constexpr UINT kWidth = 32;
constexpr UINT kHeight = 32;

void CheckHr(HRESULT hr, const char* what) {
    if (FAILED(hr)) {
        std::ostringstream oss;
        oss << what << " failed: HRESULT=0x" << std::hex << static_cast<unsigned long>(hr);
        throw std::runtime_error(oss.str());
    }
}

void Require(bool cond, const char* msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

bool IsSkippableD3D11FenceError(const std::exception& e) {
    const std::string msg = e.what();
    return msg.find("ID3D11Device5") != std::string::npos ||
           msg.find("D3D11.4")       != std::string::npos ||
           msg.find("OpenSharedFence") != std::string::npos;
}

D3DInteropLib::ComPtr<ID3D12DescriptorHeap> CreateSrvHeap(ID3D12Device* device, UINT count) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = count;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask = 0;

    D3DInteropLib::ComPtr<ID3D12DescriptorHeap> heap;
    CheckHr(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)), "CreateDescriptorHeap(CBV_SRV_UAV)");
    return heap;
}

void TestTypedSrvD3D11ToD3D12() {
    auto core11 = D3D11CoreLib::D3D11Core::CreateShared();
    auto core12 = D3D12CoreLib::D3D12Core::CreateSharedWithAdapterLuid(core11->GetAdapterLuid());

    D3DInteropLib::SharedTextureDesc desc;
    desc.width = kWidth;
    desc.height = kHeight;
    desc.format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    desc.allowRenderTarget = false;
    desc.allowUnorderedAccess = false;
    desc.sync = D3DInteropLib::SyncPolicy::SharedFence;

    auto shared = D3DInteropLib::SharedTexture::CreateOnD3D11(*core11, desc);
    auto endpoint12 = D3DInteropLib::D3D12TextureEndpoint::Open(*core12, shared);

    D3DInteropLib::D3D12Texture2DSrvOptions options;
    options.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    options.mostDetailedMip = 0;
    options.mipLevels = 1;
    options.planeSlice = 0;

    const auto srvDesc = D3DInteropLib::MakeD3D12Texture2DSrvDesc(endpoint12, options);
    Require(srvDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM, "typed SRV helper returned unexpected format");
    Require(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D, "typed SRV helper returned unexpected dimension");
    Require(srvDesc.Texture2D.MipLevels == 1, "typed SRV helper returned unexpected mip count");
    Require(srvDesc.Texture2D.PlaneSlice == 0, "typed SRV helper returned unexpected plane slice");

    auto heap = CreateSrvHeap(core12->GetDevice(), 1);
    D3DInteropLib::CreateD3D12Texture2DSrv(
        core12->GetDevice(),
        endpoint12,
        heap->GetCPUDescriptorHandleForHeapStart(),
        options);
}

} // namespace

int main() {
    try {
        TestTypedSrvD3D11ToD3D12();
    } catch (const std::exception& e) {
        if (IsSkippableD3D11FenceError(e)) {
            std::cerr << "SKIP: required D3D11.4 shared fence support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "TypedSrv11To12 passed." << std::endl;
    return 0;
}
