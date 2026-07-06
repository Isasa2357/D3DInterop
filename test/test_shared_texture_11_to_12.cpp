//
// test_shared_texture_11_to_12.cpp
// T1 fallback: D3D11 allocator -> D3D12 opener shared texture roundtrip.
//
// Notes:
//   Some drivers reject D3D12-allocated textures in ID3D11Device1::OpenSharedResource1
//   with E_INVALIDARG even when the D3D12 resource is shared. This test uses the more
//   conservative D3D11 allocator path first, then opens the NT handle from D3D12.
//
#include <D3DInterop/D3DInterop.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D12Helper/D3D12Core/D3D12Barrier.hpp>
#include <D3D12Helper/D3D12Core/D3D12Core.hpp>
#include <D3D12Helper/D3D12Framework/D3D12Resource.hpp>

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

std::vector<std::uint8_t> MakePattern(UINT width, UINT height) {
    std::vector<std::uint8_t> data(static_cast<size_t>(width) * height * 4u);
    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            const size_t i = (static_cast<size_t>(y) * width + x) * 4u;
            data[i + 0] = static_cast<std::uint8_t>((x * 13u + y * 3u) & 0xffu);
            data[i + 1] = static_cast<std::uint8_t>((x * 5u  + y * 17u) & 0xffu);
            data[i + 2] = static_cast<std::uint8_t>(((x ^ y) * 11u) & 0xffu);
            data[i + 3] = 255u;
        }
    }
    return data;
}

bool IsSkippableD3D11FenceError(const std::exception& e) {
    const std::string msg = e.what();
    return msg.find("ID3D11Device5") != std::string::npos ||
           msg.find("D3D11.4")       != std::string::npos ||
           msg.find("OpenSharedFence") != std::string::npos;
}

void WritePatternOnD3D11(D3D11CoreLib::D3D11Core& core11,
                         D3DInteropLib::D3D11TextureEndpoint& endpoint11,
                         const std::vector<std::uint8_t>& pattern) {
    D3D11_TEXTURE2D_DESC desc = {};
    endpoint11.Get()->GetDesc(&desc);
    Require(desc.Width == kWidth && desc.Height == kHeight, "D3D11 texture has unexpected size");
    Require(desc.Format == kFormat, "D3D11 texture has unexpected format");

    core11.GetImmediateContext()->UpdateSubresource(
        endpoint11.Get(),
        0,
        nullptr,
        pattern.data(),
        kWidth * 4u,
        kWidth * kHeight * 4u);
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

void VerifyPatternOnD3D12(D3D12CoreLib::D3D12Core& core12,
                          D3DInteropLib::D3D12TextureEndpoint& endpoint12,
                          const std::vector<std::uint8_t>& expected) {
    ID3D12Device* device = core12.GetDevice();
    ID3D12Resource* texture = endpoint12.Get();
    Require(texture != nullptr, "D3D12 endpoint texture is null");

    const D3D12_RESOURCE_DESC texDesc = texture->GetDesc();
    Require(texDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D, "D3D12 opened resource is not Texture2D");
    Require(texDesc.Width == kWidth && texDesc.Height == kHeight, "D3D12 opened texture has unexpected size");
    Require(texDesc.Format == kFormat, "D3D12 opened texture has unexpected format");

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
        const std::uint8_t* exp = expected.data() + static_cast<size_t>(y) * kWidth * 4u;
        if (std::memcmp(row, exp, kWidth * 4u) != 0) {
            ok = false;
        }
    }

    D3D12_RANGE writtenRange = { 0, 0 };
    readback->Unmap(0, &writtenRange);
    Require(ok, "D3D12 readback pixels do not match D3D11 uploaded pattern");
}

void TestD3D11ToD3D12SharedTexture() {
    auto core11 = D3D11CoreLib::D3D11Core::CreateShared();
    auto core12 = D3D12CoreLib::D3D12Core::CreateSharedWithAdapterLuid(core11->GetAdapterLuid());

    D3DInteropLib::SharedTextureDesc desc;
    desc.width = kWidth;
    desc.height = kHeight;
    desc.format = kFormat;
    desc.allowRenderTarget = false;
    desc.allowUnorderedAccess = false;
    desc.sync = D3DInteropLib::SyncPolicy::SharedFence;

    auto sharedTexture = D3DInteropLib::SharedTexture::CreateOnD3D11(*core11, desc);
    auto endpoint11 = D3DInteropLib::D3D11TextureEndpoint::Open(*core11, sharedTexture);
    auto endpoint12 = D3DInteropLib::D3D12TextureEndpoint::Open(*core12, sharedTexture);

    auto sharedFence = D3DInteropLib::SharedFence::CreateOnD3D11(*core11);
    auto fence11 = D3DInteropLib::D3D11FenceEndpoint::Open(*core11, sharedFence);
    auto fence12 = D3DInteropLib::D3D12FenceEndpoint::Open(*core12, sharedFence);

    const std::vector<std::uint8_t> pattern = MakePattern(kWidth, kHeight);
    WritePatternOnD3D11(*core11, endpoint11, pattern);

    fence11.Signal(core11->GetImmediateContext4(), 1);
    core11->Flush();
    fence12.CpuWait(1);

    VerifyPatternOnD3D12(*core12, endpoint12, pattern);
}

} // namespace

int main() {
    try {
        TestD3D11ToD3D12SharedTexture();
    } catch (const std::exception& e) {
        if (IsSkippableD3D11FenceError(e)) {
            std::cerr << "SKIP: required D3D11.4 shared fence support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "SharedTexture11To12 passed." << std::endl;
    return 0;
}
