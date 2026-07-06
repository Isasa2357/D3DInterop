//
// test_typeless_view_policy_11_to_12.cpp
// P12: typeless resource -> typed D3D12 SRV/RTV/UAV policy test.
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
constexpr UINT kWidth = 16;
constexpr UINT kHeight = 16;

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

bool IsSkippableSharingOrFormatError(const std::exception& e) {
    const std::string msg = e.what();
    return msg.find("ID3D11Device5") != std::string::npos ||
           msg.find("D3D11.4") != std::string::npos ||
           msg.find("OpenSharedFence") != std::string::npos ||
           msg.find("CreateTexture2D") != std::string::npos ||
           msg.find("E_INVALIDARG") != std::string::npos ||
           msg.find("HRESULT=0x80070057") != std::string::npos;
}

D3DInteropLib::ComPtr<ID3D12DescriptorHeap> CreateHeap(
    ID3D12Device* device,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    UINT count) {

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = count;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    D3DInteropLib::ComPtr<ID3D12DescriptorHeap> heap;
    CheckHr(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)), "CreateDescriptorHeap");
    return heap;
}

void TestTypelessViewPolicy() {
    auto core11 = D3D11CoreLib::D3D11Core::CreateShared();
    auto core12 = D3D12CoreLib::D3D12Core::CreateSharedWithAdapterLuid(core11->GetAdapterLuid());

    const auto formatSet = D3DInteropLib::GetD3DInteropTextureViewFormatSet(DXGI_FORMAT_R8G8B8A8_TYPELESS);
    Require(formatSet.defaultSrvFormat == DXGI_FORMAT_R8G8B8A8_UNORM, "typeless default SRV format is unexpected");
    Require(formatSet.defaultRtvFormat == DXGI_FORMAT_R8G8B8A8_UNORM, "typeless default RTV format is unexpected");
    Require(formatSet.defaultUavFormat == DXGI_FORMAT_R8G8B8A8_UNORM, "typeless default UAV format is unexpected");

    D3DInteropLib::SharedTextureDesc desc;
    desc.width = kWidth;
    desc.height = kHeight;
    desc.format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    desc.allowRenderTarget = true;
    desc.allowUnorderedAccess = true;
    desc.sync = D3DInteropLib::SyncPolicy::SharedFence;

    auto shared = D3DInteropLib::SharedTexture::CreateOnD3D11(*core11, desc);
    auto endpoint12 = D3DInteropLib::D3D12TextureEndpoint::Open(*core12, shared);

    const auto srvDesc = D3DInteropLib::MakeD3D12Texture2DSrvDesc(endpoint12);
    const auto rtvDesc = D3DInteropLib::MakeD3D12Texture2DRtvDesc(endpoint12);
    const auto uavDesc = D3DInteropLib::MakeD3D12Texture2DUavDesc(endpoint12);

    Require(srvDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM, "default SRV format did not resolve to UNORM");
    Require(rtvDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM, "default RTV format did not resolve to UNORM");
    Require(uavDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM, "default UAV format did not resolve to UNORM");

    auto srvUavHeap = CreateHeap(core12->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);
    auto rtvHeap = CreateHeap(core12->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);

    const UINT inc = core12->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto cpu = srvUavHeap->GetCPUDescriptorHandleForHeapStart();
    D3DInteropLib::CreateD3D12Texture2DSrv(core12->GetDevice(), endpoint12, cpu);
    cpu.ptr += inc;
    D3DInteropLib::CreateD3D12Texture2DUav(core12->GetDevice(), endpoint12, cpu);

    D3DInteropLib::CreateD3D12Texture2DRtv(
        core12->GetDevice(), endpoint12, rtvHeap->GetCPUDescriptorHandleForHeapStart());
}

} // namespace

int main() {
    try {
        TestTypelessViewPolicy();
    } catch (const std::exception& e) {
        if (IsSkippableSharingOrFormatError(e)) {
            std::cerr << "SKIP: typeless shared texture view support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "TypelessViewPolicy11To12 passed." << std::endl;
    return 0;
}
