//
// ring_async_no_readback_11_to_12.cpp
// Multi-slot ring channel sample with GPU waits and no CPU readback.
//
// The producer submits several frames to a D3D11 -> D3D12 ring channel. For each
// token, D3D12 queues a GPU-side wait and then copies the shared texture into a
// private D3D12 texture. The sample validates synchronization structure by running
// without CPU readback or pixel comparison. Submitted command contexts are kept
// alive until the queue is idle because D3D12 command allocators cannot be
// released while their command lists may still be executing.
//
#include <D3DInterop/D3DInterop.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D12Helper/D3D12Core/D3D12Barrier.hpp>
#include <D3D12Helper/D3D12Core/D3D12Core.hpp>

#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kSkip = 77;
constexpr UINT kWidth = 32;
constexpr UINT kHeight = 32;
constexpr UINT kFrames = 9;
constexpr std::size_t kSlots = 3;
constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

void CheckHr(HRESULT hr, const char* what) {
    if (FAILED(hr)) {
        std::ostringstream oss;
        oss << what << " failed: HRESULT=0x" << std::hex << static_cast<unsigned long>(hr);
        throw std::runtime_error(oss.str());
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

D3DInteropLib::ComPtr<ID3D12Resource> CreatePrivateD3D12Texture(
    ID3D12Device* device,
    UINT width,
    UINT height,
    DXGI_FORMAT format) {

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3DInteropLib::ComPtr<ID3D12Resource> texture;
    CheckHr(device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&texture)),
            "CreateCommittedResource(private texture)");
    return texture;
}

void WriteFrameOnD3D11(D3D11CoreLib::D3D11Core& core11,
                       ID3D11Texture2D* texture,
                       const std::vector<std::uint8_t>& frameData) {
    core11.GetImmediateContext()->UpdateSubresource(
        texture,
        0,
        nullptr,
        frameData.data(),
        kWidth * 4u,
        kWidth * kHeight * 4u);
}

D3D12CoreLib::D3D12CommandContext RecordGpuCopyFromSharedToPrivate(
    D3D12CoreLib::D3D12Core& core12,
    ID3D12Resource* sharedTexture,
    ID3D12Resource* privateTexture) {

    // The command allocator/list owned by D3D12CommandContext must remain alive
    // until the GPU finishes executing the submitted command list. Return the
    // context to the caller so it can be stored in a pending list until WaitIdle().
    D3D12CoreLib::D3D12CommandContext ctx = core12.CreateDirectContext();
    ctx.Reset();

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        sharedTexture,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_SOURCE));

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = sharedTexture;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = privateTexture;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    ctx.GetCommandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        sharedTexture,
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_COMMON));

    ctx.Close();
    ID3D12CommandList* lists[] = { ctx.GetCommandList() };
    core12.DirectQueue().ExecuteCommandLists(1, lists);
    return ctx;
}

void RunRingAsyncNoReadbackSample() {
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

    std::vector<D3DInteropLib::ComPtr<ID3D12Resource>> privateTextures;
    privateTextures.reserve(kSlots);
    for (std::size_t i = 0; i < kSlots; ++i) {
        privateTextures.push_back(CreatePrivateD3D12Texture(core12->GetDevice(), kWidth, kHeight, kFormat));
    }

    std::vector<D3D12CoreLib::D3D12CommandContext> pendingContexts;
    pendingContexts.reserve(kFrames);

    for (UINT frame = 0; frame < kFrames; ++frame) {
        auto token = ring.BeginProduce();
        const auto frameData = MakeFramePattern(frame);
        WriteFrameOnD3D11(*core11, ring.ProducerTexture(token).Get(), frameData);
        ring.EndProduce(core11->GetImmediateContext4(), token);

        // Queue the wait on the D3D12 GPU timeline, then queue GPU work after it.
        ring.WaitReadyOnConsumerGpu(core12->GetDirectCommandQueue(), token);
        pendingContexts.push_back(RecordGpuCopyFromSharedToPrivate(
            *core12,
            ring.ConsumerTexture(token).Get(),
            privateTextures[token.slotIndex].Get()));
        ring.EndConsume(core12->GetDirectCommandQueue(), token);

        std::cout << "submitted frame " << frame
                  << " slot=" << token.slotIndex
                  << " fence=" << token.fenceValue << std::endl;
    }

    ring.WaitConsumedForAllOnProducerCpu();
    core12->DirectQueue().WaitIdle();

    // Safe to release command allocators/lists after the queue is idle.
    pendingContexts.clear();
}

} // namespace

int main() {
    try {
        RunRingAsyncNoReadbackSample();
    } catch (const std::exception& e) {
        if (IsSkippableD3D11FenceError(e)) {
            std::cerr << "SKIP: required D3D11.4 shared fence support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "RingAsyncNoReadback11To12 sample passed." << std::endl;
    return 0;
}
