//
// test_texture_ring_channel_11_to_12.cpp
// P6: D3D11 allocator -> D3D12 opener multi-slot texture ring channel.
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
constexpr UINT kWidth = 24;
constexpr UINT kHeight = 24;
constexpr UINT kFrames = 7;
constexpr std::size_t kSlots = 3;
constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

void CheckHr(HRESULT hr, const char* what) {
    if (FAILED(hr)) {
        std::ostringstream oss;
        oss << what << " failed: HRESULT=0x"
            << std::hex << static_cast<unsigned long>(hr);
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

std::vector<std::uint8_t> MakeFramePattern(UINT frameIndex) {
    std::vector<std::uint8_t> data(static_cast<size_t>(kWidth) * kHeight * 4u);
    for (UINT y = 0; y < kHeight; ++y) {
        for (UINT x = 0; x < kWidth; ++x) {
            const size_t i = (static_cast<size_t>(y) * kWidth + x) * 4u;
            data[i + 0] = static_cast<std::uint8_t>((x * 17u + y * 7u  + frameIndex * 31u) & 0xffu);
            data[i + 1] = static_cast<std::uint8_t>((x * 3u  + y * 19u + frameIndex * 13u) & 0xffu);
            data[i + 2] = static_cast<std::uint8_t>(((x ^ y) * 23u + frameIndex * 5u) & 0xffu);
            data[i + 3] = 255u;
        }
    }
    return data;
}

void WriteFrameOnD3D11(D3D11CoreLib::D3D11Core& core11,
                       D3DInteropLib::D3D11TextureEndpoint& endpoint11,
                       const std::vector<std::uint8_t>& frameData) {
    D3D11_TEXTURE2D_DESC desc = {};
    endpoint11.Get()->GetDesc(&desc);
    Require(desc.Width == kWidth && desc.Height == kHeight, "D3D11 ring texture has unexpected size");
    Require(desc.Format == kFormat, "D3D11 ring texture has unexpected format");

    core11.GetImmediateContext()->UpdateSubresource(
        endpoint11.Get(),
        0,
        nullptr,
        frameData.data(),
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

void VerifyFrameOnD3D12(D3D12CoreLib::D3D12Core& core12,
                        D3DInteropLib::D3D12TextureEndpoint& endpoint12,
                        const std::vector<std::uint8_t>& expected) {
    ID3D12Device* device = core12.GetDevice();
    ID3D12Resource* texture = endpoint12.Get();
    Require(texture != nullptr, "D3D12 ring endpoint texture is null");

    const D3D12_RESOURCE_DESC texDesc = texture->GetDesc();
    Require(texDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D, "D3D12 opened ring resource is not Texture2D");
    Require(texDesc.Width == kWidth && texDesc.Height == kHeight, "D3D12 opened ring texture has unexpected size");
    Require(texDesc.Format == kFormat, "D3D12 opened ring texture has unexpected format");

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
        const std::uint8_t* row =
            bytes + layout.Offset + static_cast<size_t>(y) * layout.Footprint.RowPitch;
        const std::uint8_t* exp =
            expected.data() + static_cast<size_t>(y) * kWidth * 4u;
        if (std::memcmp(row, exp, kWidth * 4u) != 0) {
            ok = false;
        }
    }

    D3D12_RANGE writtenRange = { 0, 0 };
    readback->Unmap(0, &writtenRange);

    Require(ok, "D3D12 readback pixels do not match the D3D11 ring-channel frame");
}

void TestD3D11ToD3D12TextureRingChannel() {
    auto core11 = D3D11CoreLib::D3D11Core::CreateShared();
    auto core12 = D3D12CoreLib::D3D12Core::CreateSharedWithAdapterLuid(core11->GetAdapterLuid());

    D3DInteropLib::SharedTextureDesc desc;
    desc.width = kWidth;
    desc.height = kHeight;
    desc.format = kFormat;
    desc.allowRenderTarget = false;
    desc.allowUnorderedAccess = false;
    desc.sync = D3DInteropLib::SyncPolicy::SharedFence;

    auto ring = D3DInteropLib::D3D11ToD3D12TextureRingChannel::Create(
        *core11,
        *core12,
        desc,
        kSlots);

    Require(ring.SlotCount() == kSlots, "ring channel did not create the requested slot count");

    for (UINT frame = 0; frame < kFrames; ++frame) {
        auto token = ring.BeginProduce();
        Require(token.slotIndex == static_cast<std::size_t>(frame % kSlots),
                "ring channel selected an unexpected slot index");
        Require(token.fenceValue == static_cast<UINT64>(frame + 1u),
                "ring channel returned an unexpected fence value");

        const auto frameData = MakeFramePattern(frame);
        WriteFrameOnD3D11(*core11, ring.ProducerTexture(token), frameData);
        ring.EndProduce(core11->GetImmediateContext4(), token);

        ring.WaitReadyOnConsumerCpu(token);
        VerifyFrameOnD3D12(*core12, ring.ConsumerTexture(token), frameData);
        ring.EndConsume(core12->GetDirectCommandQueue(), token);
    }

    ring.WaitConsumedForAllOnProducerCpu();
}

} // namespace

int main() {
    try {
        TestD3D11ToD3D12TextureRingChannel();
    } catch (const std::exception& e) {
        if (IsSkippableD3D11FenceError(e)) {
            std::cerr << "SKIP: required D3D11.4 shared fence support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "TextureRingChannel11To12 passed." << std::endl;
    return 0;
}
