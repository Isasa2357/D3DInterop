//
// test_rtv_write_11_to_12.cpp
// P11: D3D11 allocator -> D3D12 opener RTV helper and render-target write test.
//
#include <D3DInterop/D3DInterop.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D12Helper/D3D12Core/D3D12Barrier.hpp>
#include <D3D12Helper/D3D12Core/D3D12Core.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kSkip = 77;
constexpr UINT kWidth = 16;
constexpr UINT kHeight = 16;
constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

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

bool IsSkippableSharingError(const std::exception& e) {
    const std::string msg = e.what();
    return msg.find("ID3D11Device5") != std::string::npos ||
           msg.find("D3D11.4")       != std::string::npos ||
           msg.find("OpenSharedResource") != std::string::npos ||
           msg.find("CreateSharedHandle") != std::string::npos;
}

D3DInteropLib::ComPtr<ID3D12DescriptorHeap> CreateRtvHeap(ID3D12Device* device, UINT count) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    desc.NumDescriptors = count;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask = 0;

    D3DInteropLib::ComPtr<ID3D12DescriptorHeap> heap;
    CheckHr(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)), "CreateDescriptorHeap(RTV)");
    return heap;
}

D3DInteropLib::ComPtr<ID3D12Resource> CreateReadbackBuffer(
    ID3D12Device* device,
    UINT64 sizeBytes) {

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = sizeBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3DInteropLib::ComPtr<ID3D12Resource> buffer;
    CheckHr(device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&buffer)),
            "CreateCommittedResource(readback)");
    return buffer;
}

void VerifyUniformRgba8(D3D12CoreLib::D3D12Core& core12,
                        ID3D12Resource* texture,
                        const std::array<std::uint8_t, 4>& expected) {
    ID3D12Device* device = core12.GetDevice();
    const D3D12_RESOURCE_DESC texDesc = texture->GetDesc();

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
    UINT numRows = 0;
    UINT64 rowSize = 0;
    UINT64 totalBytes = 0;
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, &layout, &numRows, &rowSize, &totalBytes);

    auto readback = CreateReadbackBuffer(device, totalBytes);

    D3D12CoreLib::D3D12CommandContext ctx = core12.CreateDirectContext();
    ctx.Reset();

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        texture,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_SOURCE));

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = texture;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = readback.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = layout;

    ctx.GetCommandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        texture,
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_COMMON));

    ctx.Close();
    ID3D12CommandList* lists[] = { ctx.GetCommandList() };
    core12.DirectQueue().ExecuteCommandLists(1, lists);
    core12.DirectQueue().WaitIdle();

    void* mapped = nullptr;
    D3D12_RANGE readRange = { 0, static_cast<SIZE_T>(totalBytes) };
    CheckHr(readback->Map(0, &readRange, &mapped), "Map(readback)");

    const auto* bytes = static_cast<const std::uint8_t*>(mapped);
    bool ok = true;
    for (UINT y = 0; y < kHeight && ok; ++y) {
        const std::uint8_t* row = bytes + layout.Offset + static_cast<size_t>(y) * layout.Footprint.RowPitch;
        for (UINT x = 0; x < kWidth; ++x) {
            const std::uint8_t* px = row + static_cast<size_t>(x) * 4u;
            if (std::memcmp(px, expected.data(), 4u) != 0) {
                ok = false;
                break;
            }
        }
    }

    D3D12_RANGE writtenRange = { 0, 0 };
    readback->Unmap(0, &writtenRange);
    Require(ok, "RTV clear result did not match expected RGBA8 value");
}

void TestRtvWriteD3D11ToD3D12() {
    auto core11 = D3D11CoreLib::D3D11Core::CreateShared();
    auto core12 = D3D12CoreLib::D3D12Core::CreateSharedWithAdapterLuid(core11->GetAdapterLuid());

    D3DInteropLib::SharedTextureDesc desc;
    desc.width = kWidth;
    desc.height = kHeight;
    desc.format = kFormat;
    desc.allowRenderTarget = true;
    desc.allowUnorderedAccess = false;
    desc.sync = D3DInteropLib::SyncPolicy::SharedFence;

    auto shared = D3DInteropLib::SharedTexture::CreateOnD3D11(*core11, desc);
    auto endpoint12 = D3DInteropLib::D3D12TextureEndpoint::Open(*core12, shared);

    D3DInteropLib::D3D12Texture2DRtvOptions options;
    options.format = kFormat;
    options.mipSlice = 0;
    options.planeSlice = 0;

    const auto rtvDesc = D3DInteropLib::MakeD3D12Texture2DRtvDesc(endpoint12, options);
    Require(rtvDesc.Format == kFormat, "RTV helper returned unexpected format");
    Require(rtvDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2D, "RTV helper returned unexpected dimension");
    Require(rtvDesc.Texture2D.MipSlice == 0, "RTV helper returned unexpected mip slice");

    auto rtvHeap = CreateRtvHeap(core12->GetDevice(), 1);
    const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    D3DInteropLib::CreateD3D12Texture2DRtv(core12->GetDevice(), endpoint12, rtvHandle, options);

    ID3D12Resource* texture = endpoint12.Get();
    D3D12CoreLib::D3D12CommandContext ctx = core12->CreateDirectContext();
    ctx.Reset();

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        texture,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_RENDER_TARGET));

    const FLOAT clearColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    ctx.GetCommandList()->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        texture,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_COMMON));

    ctx.Close();
    ID3D12CommandList* lists[] = { ctx.GetCommandList() };
    core12->DirectQueue().ExecuteCommandLists(1, lists);
    core12->DirectQueue().WaitIdle();

    VerifyUniformRgba8(*core12, texture, { 255u, 0u, 0u, 255u });
}

} // namespace

int main() {
    try {
        TestRtvWriteD3D11ToD3D12();
    } catch (const std::exception& e) {
        if (IsSkippableSharingError(e)) {
            std::cerr << "SKIP: required D3D11/D3D12 sharing support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "RenderTargetWrite11To12 passed." << std::endl;
    return 0;
}
