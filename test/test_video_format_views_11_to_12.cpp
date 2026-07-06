//
// test_video_format_views_11_to_12.cpp
// P12: D3D11 allocator -> D3D12 opener video format SRV descriptor tests.
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

D3DInteropLib::ComPtr<ID3D12DescriptorHeap> CreateSrvHeap(ID3D12Device* device, UINT count) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = count;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    D3DInteropLib::ComPtr<ID3D12DescriptorHeap> heap;
    CheckHr(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)), "CreateDescriptorHeap(CBV_SRV_UAV)");
    return heap;
}

D3DInteropLib::D3D12TextureEndpoint CreateVideoEndpoint(
    D3D11CoreLib::D3D11Core& core11,
    D3D12CoreLib::D3D12Core& core12,
    DXGI_FORMAT format) {

    D3DInteropLib::SharedTextureDesc desc;
    desc.width = kWidth;
    desc.height = kHeight;
    desc.format = format;
    desc.allowRenderTarget = false;
    desc.allowUnorderedAccess = false;
    desc.sync = D3DInteropLib::SyncPolicy::SharedFence;

    auto shared = D3DInteropLib::SharedTexture::CreateOnD3D11(core11, desc);
    return D3DInteropLib::D3D12TextureEndpoint::Open(core12, shared);
}

void CheckP010Views(D3D11CoreLib::D3D11Core& core11,
                    D3D12CoreLib::D3D12Core& core12) {
    auto endpoint = CreateVideoEndpoint(core11, core12, DXGI_FORMAT_P010);

    D3DInteropLib::D3D12Texture2DSrvOptions y;
    y.format = D3DInteropLib::GetD3DInteropVideoPlaneSrvFormat(DXGI_FORMAT_P010, 0);
    y.planeSlice = 0;

    D3DInteropLib::D3D12Texture2DSrvOptions uv;
    uv.format = D3DInteropLib::GetD3DInteropVideoPlaneSrvFormat(DXGI_FORMAT_P010, 1);
    uv.planeSlice = 1;

    const auto yDesc = D3DInteropLib::MakeD3D12Texture2DSrvDesc(endpoint, y);
    const auto uvDesc = D3DInteropLib::MakeD3D12Texture2DSrvDesc(endpoint, uv);

    Require(yDesc.Format == DXGI_FORMAT_R16_UNORM, "P010 Y plane SRV format is unexpected");
    Require(yDesc.Texture2D.PlaneSlice == 0, "P010 Y plane slice is unexpected");
    Require(uvDesc.Format == DXGI_FORMAT_R16G16_UNORM, "P010 UV plane SRV format is unexpected");
    Require(uvDesc.Texture2D.PlaneSlice == 1, "P010 UV plane slice is unexpected");

    auto heap = CreateSrvHeap(core12.GetDevice(), 2);
    const UINT inc = core12.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto cpu = heap->GetCPUDescriptorHandleForHeapStart();

    D3DInteropLib::CreateD3D12Texture2DSrv(core12.GetDevice(), endpoint, cpu, y);
    cpu.ptr += inc;
    D3DInteropLib::CreateD3D12Texture2DSrv(core12.GetDevice(), endpoint, cpu, uv);
}

void CheckYuy2Views(D3D11CoreLib::D3D11Core& core11,
                    D3D12CoreLib::D3D12Core& core12) {
    auto endpoint = CreateVideoEndpoint(core11, core12, DXGI_FORMAT_YUY2);

    D3DInteropLib::D3D12Texture2DSrvOptions options;
    options.format = D3DInteropLib::GetD3DInteropVideoPlaneSrvFormat(DXGI_FORMAT_YUY2, 0);
    options.planeSlice = 0;

    const auto desc = D3DInteropLib::MakeD3D12Texture2DSrvDesc(endpoint, options);
    Require(desc.Format == DXGI_FORMAT_YUY2, "YUY2 SRV format is unexpected");
    Require(desc.Texture2D.PlaneSlice == 0, "YUY2 plane slice is unexpected");

    auto heap = CreateSrvHeap(core12.GetDevice(), 1);
    D3DInteropLib::CreateD3D12Texture2DSrv(
        core12.GetDevice(), endpoint, heap->GetCPUDescriptorHandleForHeapStart(), options);
}

void TestVideoFormatViews() {
    auto core11 = D3D11CoreLib::D3D11Core::CreateShared();
    auto core12 = D3D12CoreLib::D3D12Core::CreateSharedWithAdapterLuid(core11->GetAdapterLuid());

    const auto nv12Set = D3DInteropLib::GetD3DInteropTextureViewFormatSet(DXGI_FORMAT_NV12);
    Require(nv12Set.planeCount == 2, "NV12 should expose two SRV planes");
    Require(nv12Set.planeSrvFormats[0] == DXGI_FORMAT_R8_UNORM, "NV12 plane 0 format is unexpected");
    Require(nv12Set.planeSrvFormats[1] == DXGI_FORMAT_R8G8_UNORM, "NV12 plane 1 format is unexpected");

    CheckP010Views(*core11, *core12);
    CheckYuy2Views(*core11, *core12);
}

} // namespace

int main() {
    try {
        TestVideoFormatViews();
    } catch (const std::exception& e) {
        if (IsSkippableSharingOrFormatError(e)) {
            std::cerr << "SKIP: video format sharing/view support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "VideoFormatViews11To12 passed." << std::endl;
    return 0;
}
