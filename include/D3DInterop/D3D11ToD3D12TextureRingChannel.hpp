#pragma once
//
// D3D11ToD3D12TextureRingChannel.hpp
// Multi-slot producer/consumer channel for the current validated texture quadrant:
//   D3D11 allocator / producer -> D3D12 opener / consumer.
//
// P6 builds on D3D11ToD3D12TextureChannel by allocating multiple shared textures.
// The producer can advance to another slot while older slots are waiting for the
// D3D12 consumer, reducing unnecessary producer stalls compared with a single
// shared texture ping-pong channel.
//
#include "D3D11ToD3D12TextureChannel.hpp"

#include <cstddef>
#include <vector>

namespace D3DInteropLib {

class D3D11ToD3D12TextureRingChannel {
public:
    struct Token {
        std::size_t slotIndex = 0;
        UINT64 fenceValue = 0;

        bool IsValid() const noexcept { return fenceValue != 0; }
    };

    D3D11ToD3D12TextureRingChannel() = default;
    ~D3D11ToD3D12TextureRingChannel() = default;

    static D3D11ToD3D12TextureRingChannel Create(
        D3D11CoreLib::D3D11Core& producer11,
        D3D12CoreLib::D3D12Core& consumer12,
        const SharedTextureDesc& desc,
        std::size_t slotCount = 3);

    D3D11ToD3D12TextureRingChannel(D3D11ToD3D12TextureRingChannel&&) noexcept = default;
    D3D11ToD3D12TextureRingChannel& operator=(D3D11ToD3D12TextureRingChannel&&) noexcept = default;

    D3D11ToD3D12TextureRingChannel(const D3D11ToD3D12TextureRingChannel&) = delete;
    D3D11ToD3D12TextureRingChannel& operator=(const D3D11ToD3D12TextureRingChannel&) = delete;

    std::size_t SlotCount() const noexcept { return m_slots.size(); }
    UINT64 NextFenceValue() const noexcept { return m_nextFenceValue; }

    // Producer side: select the next ring slot. If that slot still contains an
    // unconsumed frame from a previous cycle, this waits for its consumed fence.
    Token BeginProduce();

    D3D11TextureEndpoint& ProducerTexture(const Token& token);
    const D3D11TextureEndpoint& ProducerTexture(const Token& token) const;

    void EndProduce(ID3D11DeviceContext4* producerContext,
                    const Token& token,
                    bool flush = true);

    // Consumer side: wait for a token to become ready, then read from the D3D12
    // endpoint returned by ConsumerTexture(token).
    void WaitReadyOnConsumerCpu(const Token& token);
    void WaitReadyOnConsumerGpu(ID3D12CommandQueue* consumerQueue, const Token& token);

    D3D12TextureEndpoint& ConsumerTexture(const Token& token);
    const D3D12TextureEndpoint& ConsumerTexture(const Token& token) const;

    void EndConsume(ID3D12CommandQueue* consumerQueue, const Token& token);

    // Producer side: useful during shutdown or when the application wants to
    // guarantee that all submitted frames have been consumed.
    void WaitConsumedForAllOnProducerCpu();

private:
    struct Slot {
        D3D11ToD3D12TextureChannel channel;
        UINT64 lastFenceValue = 0;
    };

    Slot&       CheckedSlot(const Token& token);
    const Slot& CheckedSlot(const Token& token) const;

    std::vector<Slot> m_slots;
    std::size_t m_nextProducerSlot = 0;
    UINT64 m_nextFenceValue = 1;
};

} // namespace D3DInteropLib
