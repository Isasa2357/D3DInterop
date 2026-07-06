//
// test_texture_channel_11_to_12.cpp
// P5: reusable D3D11 -> D3D12 texture channel test.
//
#include <D3DInterop/D3DInterop.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D12Helper/D3D12Core/D3D12Barrier.hpp>
#include <D3D12Helper/D3D12Core/D3D12Core.hpp>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kSkip = 77;
constexpr UINT kWidth = 32;
constexpr UINT kHeight = 16;
constexpr UINT kFrames = 6;
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
            data[i + 0] = static_cast<std::uint8_t>((x * 9u  + y * 7u  + frameIndex * 31u) & 0xffu);
            data[i + 1] = static_cast<std::uint8_t>((x * 3u  + y * 19u + frameIndex * 13u) & 0xffu);
            data[i + 2] = static_cast<std::uint8_t>(((x ^ y) * 5u + frameIndex * 17u) & 0xffu);
            data[i + 3] = 255u;
        }
    }
    return data;
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

void WriteFrameOnProducer(D3D11CoreLib::D3D11Core& core11,
                          D3DInteropLib::D3D11ToD3D12TextureChannel& channel,
                          const std::vector<std::uint8_t>& frameData) {
    ID3D11Texture2D* texture = channel.ProducerTexture().Get();
    Require(texture != nullptr, "producer texture is null");

    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);
    Require(desc.Width == kWidth && desc.Height == kHeight, "producer texture has unexpected size");
    Require(desc.Format == kFormat, "producer texture has unexpected format");

    core11.GetImmediateContext()->UpdateSubresource(
        texture,
        0,
        nullptr,
        frameData.data(),
        kWidth * 4u,
        kWidth * kHeight * 4u);
}

void VerifyFrameOnConsumer(D3D12CoreLib::D3D12Core& core12,
                           D3DInteropLib::D3D11ToD3D12TextureChannel& channel,
                           const std::vector<std::uint8_t>& expected) {
    ID3D12Device* device = core12.GetDevice();
    ID3D12Resource* texture = channel.ConsumerTexture().Get();
    Require(texture != nullptr, "consumer texture is null");

    const D3D12_RESOURCE_DESC texDesc = texture->GetDesc();
    Require(texDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D, "consumer resource is not Texture2D");
    Require(texDesc.Width == kWidth && texDesc.Height == kHeight, "consumer texture has unexpected size");
    Require(texDesc.Format == kFormat, "consumer texture has unexpected format");

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
    Require(ok, "consumer readback does not match producer frame");
}

void TestTextureChannel() {
    auto core11 = D3D11CoreLib::D3D11Core::CreateShared();
    auto core12 = D3D12CoreLib::D3D12Core::CreateSharedWithAdapterLuid(core11->GetAdapterLuid());

    D3DInteropLib::SharedTextureDesc desc;
    desc.width = kWidth;
    desc.height = kHeight;
    desc.format = kFormat;
    desc.allowRenderTarget = false;
    desc.allowUnorderedAccess = false;
    desc.sync = D3DInteropLib::SyncPolicy::SharedFence;

    auto channel = D3DInteropLib::D3D11ToD3D12TextureChannel::Create(*core11, *core12, desc);

    for (UINT frame = 0; frame < kFrames; ++frame) {
        const UINT64 fenceValue = channel.BeginProduce();
        const auto frameData = MakeFramePattern(frame);

        WriteFrameOnProducer(*core11, channel, frameData);
        channel.EndProduce(core11->GetImmediateContext4(), fenceValue);

        channel.WaitReadyOnConsumerCpu(fenceValue);
        VerifyFrameOnConsumer(*core12, channel, frameData);
        channel.EndConsume(core12->GetDirectCommandQueue(), fenceValue);
    }

    channel.WaitConsumedOnProducerCpu(kFrames);
}

} // namespace

int main() {
    try {
        TestTextureChannel();
    } catch (const std::exception& e) {
        if (IsSkippableD3D11FenceError(e)) {
            std::cerr << "SKIP: required D3D11.4 shared fence support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "TextureChannel11To12 passed." << std::endl;
    return 0;
}
