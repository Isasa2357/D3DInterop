//
// test_nv12_video_texture_11_to_12.cpp
// P10: D3D11 allocator -> D3D12 opener NV12 shared video texture test.
//
#include <D3DInterop/D3DInterop.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D12Helper/D3D12Core/D3D12Barrier.hpp>
#include <D3D12Helper/D3D12Core/D3D12Core.hpp>

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
constexpr UINT kWidth = 64;
constexpr UINT kHeight = 32;
constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_NV12;

static_assert((kWidth % 2u) == 0u, "NV12 width must be even");
static_assert((kHeight % 2u) == 0u, "NV12 height must be even");

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

std::vector<std::uint8_t> MakeNv12Pattern() {
    std::vector<std::uint8_t> data(static_cast<size_t>(kWidth) * kHeight * 3u / 2u);

    // Plane 0: Y, one byte per pixel.
    for (UINT y = 0; y < kHeight; ++y) {
        for (UINT x = 0; x < kWidth; ++x) {
            data[static_cast<size_t>(y) * kWidth + x] =
                static_cast<std::uint8_t>((x * 3u + y * 11u) & 0xffu);
        }
    }

    // Plane 1: interleaved UV, one byte per component, kHeight / 2 rows.
    const size_t uvBase = static_cast<size_t>(kWidth) * kHeight;
    for (UINT y = 0; y < kHeight / 2u; ++y) {
        for (UINT x = 0; x < kWidth; x += 2u) {
            const size_t i = uvBase + static_cast<size_t>(y) * kWidth + x;
            data[i + 0] = static_cast<std::uint8_t>((128u + x * 5u + y * 7u) & 0xffu); // U
            data[i + 1] = static_cast<std::uint8_t>((64u  + x * 3u + y * 13u) & 0xffu); // V
        }
    }

    return data;
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

D3DInteropLib::ComPtr<ID3D12Resource> CreateReadbackBuffer(ID3D12Device* device, UINT64 sizeBytes) {
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

void CopyAndVerifyPlane(D3D12CoreLib::D3D12Core& core12,
                        ID3D12Resource* texture,
                        UINT subresourceIndex,
                        const std::uint8_t* expected,
                        UINT rowBytes,
                        UINT rows) {
    ID3D12Device* device = core12.GetDevice();
    const D3D12_RESOURCE_DESC texDesc = texture->GetDesc();

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
    UINT numRows = 0;
    UINT64 rowSize = 0;
    UINT64 totalBytes = 0;
    device->GetCopyableFootprints(&texDesc, subresourceIndex, 1, 0, &layout, &numRows, &rowSize, &totalBytes);

    Require(numRows >= rows, "NV12 plane footprint has fewer rows than expected");
    Require(rowSize >= rowBytes, "NV12 plane footprint row size is smaller than expected");

    auto readback = CreateReadbackBuffer(device, totalBytes);

    D3D12CoreLib::D3D12CommandContext ctx = core12.CreateDirectContext();
    ctx.Reset();

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = texture;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = subresourceIndex;

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = readback.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = layout;

    ctx.GetCommandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    ctx.Close();

    ID3D12CommandList* lists[] = { ctx.GetCommandList() };
    core12.DirectQueue().ExecuteCommandLists(1, lists);
    core12.DirectQueue().WaitIdle();

    void* mapped = nullptr;
    D3D12_RANGE readRange = { 0, static_cast<SIZE_T>(totalBytes) };
    CheckHr(readback->Map(0, &readRange, &mapped), "Map(readback)");

    const auto* bytes = static_cast<const std::uint8_t*>(mapped);
    bool ok = true;
    for (UINT y = 0; y < rows && ok; ++y) {
        const std::uint8_t* row =
            bytes + layout.Offset + static_cast<size_t>(y) * layout.Footprint.RowPitch;
        const std::uint8_t* exp = expected + static_cast<size_t>(y) * rowBytes;
        if (std::memcmp(row, exp, rowBytes) != 0) {
            ok = false;
        }
    }

    D3D12_RANGE writtenRange = { 0, 0 };
    readback->Unmap(0, &writtenRange);

    Require(ok, "NV12 D3D12 readback plane does not match D3D11 written data");
}

void TestNv12VideoTextureD3D11ToD3D12() {
    auto core11 = D3D11CoreLib::D3D11Core::CreateShared();
    auto core12 = D3D12CoreLib::D3D12Core::CreateSharedWithAdapterLuid(core11->GetAdapterLuid());

    D3DInteropLib::SharedTextureDesc desc;
    desc.width = kWidth;
    desc.height = kHeight;
    desc.format = kFormat;
    desc.allowRenderTarget = false;
    desc.allowUnorderedAccess = false;
    desc.sync = D3DInteropLib::SyncPolicy::SharedFence;

    auto shared = D3DInteropLib::SharedTexture::CreateOnD3D11(*core11, desc);
    auto endpoint11 = D3DInteropLib::D3D11TextureEndpoint::Open(*core11, shared);
    auto endpoint12 = D3DInteropLib::D3D12TextureEndpoint::Open(*core12, shared);

    auto readyFence = D3DInteropLib::SharedFence::CreateOnD3D11(*core11);
    auto ready11 = D3DInteropLib::D3D11FenceEndpoint::Open(*core11, readyFence);
    auto ready12 = D3DInteropLib::D3D12FenceEndpoint::Open(*core12, readyFence);

    const auto nv12 = MakeNv12Pattern();
    core11->GetImmediateContext()->UpdateSubresource(
        endpoint11.Get(),
        0,
        nullptr,
        nv12.data(),
        kWidth,
        kWidth * kHeight * 3u / 2u);

    ready11.Signal(core11->GetImmediateContext4(), 1);
    core11->GetImmediateContext4()->Flush();
    ready12.CpuWait(1);

    ID3D12Device* device = core12->GetDevice();
    ID3D12Resource* texture = endpoint12.Get();
    Require(texture != nullptr, "D3D12 NV12 endpoint texture is null");

    const D3D12_RESOURCE_DESC openedDesc = texture->GetDesc();
    Require(openedDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D, "opened NV12 resource is not Texture2D");
    Require(openedDesc.Format == DXGI_FORMAT_NV12, "opened resource format is not NV12");
    Require(openedDesc.Width == kWidth && openedDesc.Height == kHeight, "opened NV12 resource has unexpected size");

    auto srvHeap = CreateSrvHeap(device, 2);
    D3D12_CPU_DESCRIPTOR_HANDLE yHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE uvHandle = yHandle;
    uvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3DInteropLib::D3D12Texture2DSrvOptions ySrv;
    ySrv.format = DXGI_FORMAT_R8_UNORM;
    ySrv.planeSlice = 0;
    D3DInteropLib::CreateD3D12Texture2DSrv(device, endpoint12, yHandle, ySrv);

    D3DInteropLib::D3D12Texture2DSrvOptions uvSrv;
    uvSrv.format = DXGI_FORMAT_R8G8_UNORM;
    uvSrv.planeSlice = 1;
    D3DInteropLib::CreateD3D12Texture2DSrv(device, endpoint12, uvHandle, uvSrv);

    D3D12CoreLib::D3D12CommandContext barrierCtx = core12->CreateDirectContext();
    barrierCtx.Reset();
    barrierCtx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        texture,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_SOURCE));
    barrierCtx.Close();
    ID3D12CommandList* barrierLists[] = { barrierCtx.GetCommandList() };
    core12->DirectQueue().ExecuteCommandLists(1, barrierLists);
    core12->DirectQueue().WaitIdle();

    const std::uint8_t* yExpected = nv12.data();
    const std::uint8_t* uvExpected = nv12.data() + static_cast<size_t>(kWidth) * kHeight;

    CopyAndVerifyPlane(*core12, texture, 0, yExpected, kWidth, kHeight);
    CopyAndVerifyPlane(*core12, texture, 1, uvExpected, kWidth, kHeight / 2u);

    D3D12CoreLib::D3D12CommandContext restoreCtx = core12->CreateDirectContext();
    restoreCtx.Reset();
    restoreCtx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        texture,
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_COMMON));
    restoreCtx.Close();
    ID3D12CommandList* restoreLists[] = { restoreCtx.GetCommandList() };
    core12->DirectQueue().ExecuteCommandLists(1, restoreLists);
    core12->DirectQueue().WaitIdle();
}

} // namespace

int main() {
    try {
        TestNv12VideoTextureD3D11ToD3D12();
    } catch (const std::exception& e) {
        if (IsSkippableD3D11FenceError(e)) {
            std::cerr << "SKIP: required D3D11.4 shared fence support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Nv12VideoTexture11To12 passed." << std::endl;
    return 0;
}
