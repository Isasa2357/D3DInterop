#pragma once
//
// D3D11ToD3D11KeyedMutexChannel.hpp
// Reusable producer/consumer channel for D3D11 allocator / producer -> D3D11 opener / consumer.
//
// Synchronization protocol per frame:
//   producer Acquire(0) -> write -> Release(1)
//   consumer Acquire(1) -> read  -> Release(0)
//

#include "D3D11Endpoint.hpp"
#include "SharedTexture.hpp"

namespace D3DInteropLib {

class D3D11ToD3D11KeyedMutexChannel {
public:
    D3D11ToD3D11KeyedMutexChannel() = default;
    ~D3D11ToD3D11KeyedMutexChannel() = default;

    static D3D11ToD3D11KeyedMutexChannel Create(
        D3D11CoreLib::D3D11Core& producer11,
        D3D11CoreLib::D3D11Core& consumer11,
        const SharedTextureDesc& desc);

    D3D11ToD3D11KeyedMutexChannel(D3D11ToD3D11KeyedMutexChannel&&) noexcept = default;
    D3D11ToD3D11KeyedMutexChannel& operator=(D3D11ToD3D11KeyedMutexChannel&&) noexcept = default;

    D3D11ToD3D11KeyedMutexChannel(const D3D11ToD3D11KeyedMutexChannel&) = delete;
    D3D11ToD3D11KeyedMutexChannel& operator=(const D3D11ToD3D11KeyedMutexChannel&) = delete;

    // Producer side: acquire key 0 so the producer can safely write the shared texture.
    void BeginProduce(DWORD timeoutMs = INFINITE);

    // Producer side texture. Valid after BeginProduce() and before EndProduce().
    D3D11TextureEndpoint& ProducerTexture() noexcept { return m_producerTexture11; }
    const D3D11TextureEndpoint& ProducerTexture() const noexcept { return m_producerTexture11; }

    // Producer side: optionally flush producerContext, then release key 1 for the consumer.
    void EndProduce(ID3D11DeviceContext* producerContext, bool flush = true);

    // Consumer side: acquire key 1 so the consumer can safely read the shared texture.
    void BeginConsume(DWORD timeoutMs = INFINITE);

    // Consumer side texture. Valid after BeginConsume() and before EndConsume().
    D3D11TextureEndpoint& ConsumerTexture() noexcept { return m_consumerTexture11; }
    const D3D11TextureEndpoint& ConsumerTexture() const noexcept { return m_consumerTexture11; }

    // Consumer side: optionally flush consumerContext, then release key 0 for the producer.
    void EndConsume(ID3D11DeviceContext* consumerContext = nullptr, bool flush = false);

    SharedTexture& Shared() noexcept { return m_texture; }
    const SharedTexture& Shared() const noexcept { return m_texture; }

private:
    static constexpr UINT64 kProducerKey = 0;
    static constexpr UINT64 kConsumerKey = 1;

    SharedTexture m_texture;
    D3D11TextureEndpoint m_producerTexture11;
    D3D11TextureEndpoint m_consumerTexture11;
};

} // namespace D3DInteropLib
