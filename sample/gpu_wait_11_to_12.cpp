//
// gpu_wait_11_to_12.cpp
// Practical GPU-wait sample for the validated path:
//   D3D11 allocator / producer -> D3D12 opener / consumer.
//
// This sample intentionally avoids CPU readback. D3D12 waits on the shared fence
// directly on its command queue, then performs a GPU-side copy from the shared
// texture to a private D3D12 texture.
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
#include <vector>

namespace {

constexpr int kSkip = 77;
constexpr UINT kWidth = 32;
constexpr UINT kHeight = 32;
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
            data[i + 0] = static_cast<std::uint8_t>((x * 13u + y * 3u + frameIndex * 19u) & 0xffu);
            data[i + 1] = static_cast<std::uint8_t>((x * 5u  + y * 17u + frameIndex * 23u) & 0xffu);
            data[i + 2] = static_cast<std::uint8_t>(((x ^ y) * 11u + frameIndex * 29u) & 0xffu);
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

void RecordGpuCopyFromSharedToPrivate(
    D3D12CoreLib::D3D12Core& core12,
    ID3D12Resource* sharedTexture,
    ID3D12Resource* privateTexture) {

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
}

void RunGpuWaitSample() {
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
    auto privateTexture = CreatePrivateD3D12Texture(core12->GetDevice(), kWidth, kHeight, kFormat);

    const UINT64 fenceValue = channel.BeginProduce();
    const auto frameData = MakeFramePattern(0);
    WriteFrameOnD3D11(*core11, channel.ProducerTexture().Get(), frameData);
    channel.EndProduce(core11->GetImmediateContext4(), fenceValue);

    // The important part of this sample: wait is queued on the D3D12 GPU timeline.
    channel.WaitReadyOnConsumerGpu(core12->GetDirectCommandQueue(), fenceValue);

    // This copy is ordered after the GPU wait above.
    RecordGpuCopyFromSharedToPrivate(
        *core12,
        channel.ConsumerTexture().Get(),
        privateTexture.Get());

    channel.EndConsume(core12->GetDirectCommandQueue(), fenceValue);
    channel.WaitConsumedOnProducerCpu(fenceValue);
    core12->DirectQueue().WaitIdle();
}

} // namespace

int main() {
    try {
        RunGpuWaitSample();
    } catch (const std::exception& e) {
        if (IsSkippableD3D11FenceError(e)) {
            std::cerr << "SKIP: required D3D11.4 shared fence support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "GpuWait11To12 sample passed." << std::endl;
    return 0;
}
