#pragma once
//
// D3D11ToD3D12TextureChannel.hpp
// Reusable producer/consumer channel for the current validated texture quadrant:
//   D3D11 allocator / producer -> D3D12 opener / consumer.
//
// This class intentionally does not hide resource access. The application still writes
// the D3D11 texture and reads the D3D12 resource directly, while this class owns the
// shared texture and the two shared-fence timelines used for ping-pong synchronization.
//
#include "D3D11Endpoint.hpp"
#include "D3D12Endpoint.hpp"
#include "SharedFence.hpp"
#include "SharedTexture.hpp"

namespace D3DInteropLib {

class D3D11ToD3D12TextureChannel {
public:
    D3D11ToD3D12TextureChannel() = default;
    ~D3D11ToD3D12TextureChannel() = default;

    static D3D11ToD3D12TextureChannel Create(
        D3D11CoreLib::D3D11Core& producer11,
        D3D12CoreLib::D3D12Core& consumer12,
        const SharedTextureDesc& desc);

    D3D11ToD3D12TextureChannel(D3D11ToD3D12TextureChannel&&) noexcept = default;
    D3D11ToD3D12TextureChannel& operator=(D3D11ToD3D12TextureChannel&&) noexcept = default;

    D3D11ToD3D12TextureChannel(const D3D11ToD3D12TextureChannel&) = delete;
    D3D11ToD3D12TextureChannel& operator=(const D3D11ToD3D12TextureChannel&) = delete;

    // Producer side: wait until the previous frame is consumed and return a new fence value.
    UINT64 BeginProduce();

    // Producer side: signal that the frame identified by fenceValue is ready.
    // For immediate D3D11 contexts, flush=true is the safest default because it pushes the
    // UpdateSubresource and Signal commands to the GPU before D3D12 waits.
    void EndProduce(ID3D11DeviceContext4* producerContext, UINT64 fenceValue, bool flush = true);

    // Consumer side: wait for a frame to become ready.
    void WaitReadyOnConsumerCpu(UINT64 fenceValue);
    void WaitReadyOnConsumerGpu(ID3D12CommandQueue* consumerQueue, UINT64 fenceValue);

    // Consumer side: signal that the frame has been consumed and the producer may overwrite it.
    void EndConsume(ID3D12CommandQueue* consumerQueue, UINT64 fenceValue);

    // Producer side: explicit wait helper, useful at shutdown or for external pacing.
    void WaitConsumedOnProducerCpu(UINT64 fenceValue);

    UINT64 NextFenceValue() const noexcept { return m_nextFenceValue; }

    SharedTexture& Shared() noexcept { return m_texture; }
    const SharedTexture& Shared() const noexcept { return m_texture; }

    D3D11TextureEndpoint& ProducerTexture() noexcept { return m_producerTexture11; }
    const D3D11TextureEndpoint& ProducerTexture() const noexcept { return m_producerTexture11; }

    D3D12TextureEndpoint& ConsumerTexture() noexcept { return m_consumerTexture12; }
    const D3D12TextureEndpoint& ConsumerTexture() const noexcept { return m_consumerTexture12; }

    D3D11FenceEndpoint& ReadyProducerFence() noexcept { return m_ready11; }
    D3D12FenceEndpoint& ReadyConsumerFence() noexcept { return m_ready12; }
    D3D12FenceEndpoint& ConsumedConsumerFence() noexcept { return m_consumed12; }
    D3D11FenceEndpoint& ConsumedProducerFence() noexcept { return m_consumed11; }

private:
    SharedTexture m_texture;
    D3D11TextureEndpoint m_producerTexture11;
    D3D12TextureEndpoint m_consumerTexture12;

    // ready: producer D3D11 -> consumer D3D12
    SharedFence m_readyFence;
    D3D11FenceEndpoint m_ready11;
    D3D12FenceEndpoint m_ready12;

    // consumed: consumer D3D12 -> producer D3D11
    SharedFence m_consumedFence;
    D3D12FenceEndpoint m_consumed12;
    D3D11FenceEndpoint m_consumed11;

    UINT64 m_nextFenceValue = 1;
};

} // namespace D3DInteropLib
