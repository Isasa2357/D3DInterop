#pragma once
//
// D3D11ToD3D11KeyedMutexRingChannel.hpp
// Multi-slot D3D11 -> D3D11 KeyedMutex producer/consumer channel.
//
// Each slot is an independent D3D11ToD3D11KeyedMutexChannel.
// A producer can advance to the next slot while older slots are waiting for the consumer.
// When the producer wraps around to a still-unconsumed slot, BeginProduce() naturally waits
// on that slot's key 0.
//
#include "D3D11ToD3D11KeyedMutexChannel.hpp"

#include <cstddef>
#include <vector>

namespace D3DInteropLib {

class D3D11ToD3D11KeyedMutexRingChannel {
public:
    struct Token {
        std::size_t slotIndex = 0;
        UINT64 sequence = 0;

        bool IsValid() const noexcept { return sequence != 0; }
    };

    D3D11ToD3D11KeyedMutexRingChannel() = default;
    ~D3D11ToD3D11KeyedMutexRingChannel() = default;

    static D3D11ToD3D11KeyedMutexRingChannel Create(
        D3D11CoreLib::D3D11Core& producer11,
        D3D11CoreLib::D3D11Core& consumer11,
        const SharedTextureDesc& desc,
        std::size_t slotCount = 3);

    D3D11ToD3D11KeyedMutexRingChannel(D3D11ToD3D11KeyedMutexRingChannel&&) noexcept = default;
    D3D11ToD3D11KeyedMutexRingChannel& operator=(D3D11ToD3D11KeyedMutexRingChannel&&) noexcept = default;

    D3D11ToD3D11KeyedMutexRingChannel(const D3D11ToD3D11KeyedMutexRingChannel&) = delete;
    D3D11ToD3D11KeyedMutexRingChannel& operator=(const D3D11ToD3D11KeyedMutexRingChannel&) = delete;

    std::size_t SlotCount() const noexcept { return m_slots.size(); }
    UINT64 NextSequence() const noexcept { return m_nextSequence; }

    // Producer side: select and acquire the next slot.
    Token BeginProduce(DWORD timeoutMs = INFINITE);

    D3D11TextureEndpoint& ProducerTexture(const Token& token);
    const D3D11TextureEndpoint& ProducerTexture(const Token& token) const;

    void EndProduce(const Token& token,
                    ID3D11DeviceContext* producerContext,
                    bool flush = true);

    // Consumer side: acquire the slot identified by token.
    void BeginConsume(const Token& token, DWORD timeoutMs = INFINITE);

    D3D11TextureEndpoint& ConsumerTexture(const Token& token);
    const D3D11TextureEndpoint& ConsumerTexture(const Token& token) const;

    void EndConsume(const Token& token,
                    ID3D11DeviceContext* consumerContext = nullptr,
                    bool flush = false);

private:
    struct Slot {
        D3D11ToD3D11KeyedMutexChannel channel;
        UINT64 activeSequence = 0;
    };

    Slot&       CheckedSlot(const Token& token);
    const Slot& CheckedSlot(const Token& token) const;

    std::vector<Slot> m_slots;
    std::size_t m_nextProducerSlot = 0;
    UINT64 m_nextSequence = 1;
};

} // namespace D3DInteropLib
