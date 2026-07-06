//
// test_uav_write_11_to_12.cpp
// P11: D3D11 allocator -> D3D12 opener UAV helper and unordered-access write test.
//
#include <D3DInterop/D3DInterop.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D12Helper/D3D12Core/D3D12Barrier.hpp>
#include <D3D12Helper/D3D12Core/D3D12Core.hpp>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

constexpr int kSkip = 77;
constexpr UINT kWidth = 16;
constexpr UINT kHeight = 16;
constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R32_UINT;
constexpr std::uint32_t kClearValue = 0x11223344u;

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

D3DInteropLib::ComPtr<ID3D12DescriptorHeap> CreateShaderVisibleUavHeap(ID3D12Device* device, UINT count) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = count;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    desc.NodeMask = 0;

    D3DInteropLib::ComPtr<ID3D12DescriptorHeap> heap;
    CheckHr(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)), "CreateDescriptorHeap(CBV_SRV_UAV)");
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

void VerifyUniformR32Uint(D3D12CoreLib::D3D12Core& core12,
                          ID3D12Resource* texture,
                          std::uint32_t expected) {
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
        const auto* row = reinterpret_cast<const std::uint32_t*>(
            bytes + layout.Offset + static_cast<size_t>(y) * layout.Footprint.RowPitch);
        for (UINT x = 0; x < kWidth; ++x) {
            if (row[x] != expected) {
                ok = false;
                break;
            }
        }
    }

    D3D12_RANGE writtenRange = { 0, 0 };
    readback->Unmap(0, &writtenRange);
    Require(ok, "UAV clear result did not match expected R32_UINT value");
}

void TestUavWriteD3D11ToD3D12() {
    auto core11 = D3D11CoreLib::D3D11Core::CreateShared();
    auto core12 = D3D12CoreLib::D3D12Core::CreateSharedWithAdapterLuid(core11->GetAdapterLuid());

    D3DInteropLib::SharedTextureDesc desc;
    desc.width = kWidth;
    desc.height = kHeight;
    desc.format = kFormat;
    desc.allowRenderTarget = false;
    desc.allowUnorderedAccess = true;
    desc.sync = D3DInteropLib::SyncPolicy::SharedFence;

    auto shared = D3DInteropLib::SharedTexture::CreateOnD3D11(*core11, desc);
    auto endpoint12 = D3DInteropLib::D3D12TextureEndpoint::Open(*core12, shared);

    D3DInteropLib::D3D12Texture2DUavOptions options;
    options.format = kFormat;
    options.mipSlice = 0;
    options.planeSlice = 0;

    const auto uavDesc = D3DInteropLib::MakeD3D12Texture2DUavDesc(endpoint12, options);
    Require(uavDesc.Format == kFormat, "UAV helper returned unexpected format");
    Require(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2D, "UAV helper returned unexpected dimension");
    Require(uavDesc.Texture2D.MipSlice == 0, "UAV helper returned unexpected mip slice");

    auto uavHeap = CreateShaderVisibleUavHeap(core12->GetDevice(), 1);
    const D3D12_CPU_DESCRIPTOR_HANDLE uavCpu = uavHeap->GetCPUDescriptorHandleForHeapStart();
    const D3D12_GPU_DESCRIPTOR_HANDLE uavGpu = uavHeap->GetGPUDescriptorHandleForHeapStart();
    D3DInteropLib::CreateD3D12Texture2DUav(core12->GetDevice(), endpoint12, uavCpu, options);

    ID3D12Resource* texture = endpoint12.Get();
    D3D12CoreLib::D3D12CommandContext ctx = core12->CreateDirectContext();
    ctx.Reset();

    ID3D12DescriptorHeap* heaps[] = { uavHeap.Get() };
    ctx.GetCommandList()->SetDescriptorHeaps(1, heaps);

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        texture,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    const UINT clearValues[4] = { kClearValue, 0u, 0u, 0u };
    ctx.GetCommandList()->ClearUnorderedAccessViewUint(
        uavGpu,
        uavCpu,
        texture,
        clearValues,
        0,
        nullptr);

    ctx.ResourceBarrier(D3D12CoreLib::MakeUavBarrier(texture));

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        texture,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON));

    ctx.Close();
    ID3D12CommandList* lists[] = { ctx.GetCommandList() };
    core12->DirectQueue().ExecuteCommandLists(1, lists);
    core12->DirectQueue().WaitIdle();

    VerifyUniformR32Uint(*core12, texture, kClearValue);
}

} // namespace

int main() {
    try {
        TestUavWriteD3D11ToD3D12();
    } catch (const std::exception& e) {
        if (IsSkippableSharingError(e)) {
            std::cerr << "SKIP: required D3D11/D3D12 sharing support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "UnorderedAccessWrite11To12 passed." << std::endl;
    return 0;
}
