//
// D3D11ToD3D11KeyedMutexRingChannel.cpp
//
#include "D3D11ToD3D11KeyedMutexRingChannel.hpp"

#include <stdexcept>
#include <utility>

namespace D3DInteropLib {

D3D11ToD3D11KeyedMutexRingChannel D3D11ToD3D11KeyedMutexRingChannel::Create(
    D3D11CoreLib::D3D11Core& producer11,
    D3D11CoreLib::D3D11Core& consumer11,
    const SharedTextureDesc& desc,
    std::size_t slotCount) {

    if (slotCount == 0) {
        throw std::runtime_error("D3D11ToD3D11KeyedMutexRingChannel::Create: slotCount must be > 0");
    }

    ThrowIfAdapterMismatch(producer11.GetAdapterLuid(),
                           consumer11.GetAdapterLuid(),
                           "D3D11ToD3D11KeyedMutexRingChannel::Create");

    if (desc.sync != SyncPolicy::KeyedMutex) {
        throw std::runtime_error(
            "D3D11ToD3D11KeyedMutexRingChannel::Create: only KeyedMutex sync is supported");
    }

    D3D11ToD3D11KeyedMutexRingChannel ring;
    ring.m_slots.reserve(slotCount);
    for (std::size_t i = 0; i < slotCount; ++i) {
        Slot slot;
        slot.channel = D3D11ToD3D11KeyedMutexChannel::Create(producer11, consumer11, desc);
        slot.activeSequence = 0;
        ring.m_slots.push_back(std::move(slot));
    }

    ring.m_nextProducerSlot = 0;
    ring.m_nextSequence = 1;
    return ring;
}

D3D11ToD3D11KeyedMutexRingChannel::Token
D3D11ToD3D11KeyedMutexRingChannel::BeginProduce(DWORD timeoutMs) {
    if (m_slots.empty()) {
        throw std::runtime_error("D3D11ToD3D11KeyedMutexRingChannel::BeginProduce: channel is not initialized");
    }

    Slot& slot = m_slots[m_nextProducerSlot];
    slot.channel.BeginProduce(timeoutMs);

    Token token;
    token.slotIndex = m_nextProducerSlot;
    token.sequence = m_nextSequence++;

    slot.activeSequence = token.sequence;
    m_nextProducerSlot = (m_nextProducerSlot + 1) % m_slots.size();
    return token;
}

D3D11TextureEndpoint& D3D11ToD3D11KeyedMutexRingChannel::ProducerTexture(const Token& token) {
    return CheckedSlot(token).channel.ProducerTexture();
}

const D3D11TextureEndpoint& D3D11ToD3D11KeyedMutexRingChannel::ProducerTexture(const Token& token) const {
    return CheckedSlot(token).channel.ProducerTexture();
}

void D3D11ToD3D11KeyedMutexRingChannel::EndProduce(
    const Token& token,
    ID3D11DeviceContext* producerContext,
    bool flush) {

    CheckedSlot(token).channel.EndProduce(producerContext, flush);
}

void D3D11ToD3D11KeyedMutexRingChannel::BeginConsume(const Token& token, DWORD timeoutMs) {
    CheckedSlot(token).channel.BeginConsume(timeoutMs);
}

D3D11TextureEndpoint& D3D11ToD3D11KeyedMutexRingChannel::ConsumerTexture(const Token& token) {
    return CheckedSlot(token).channel.ConsumerTexture();
}

const D3D11TextureEndpoint& D3D11ToD3D11KeyedMutexRingChannel::ConsumerTexture(const Token& token) const {
    return CheckedSlot(token).channel.ConsumerTexture();
}

void D3D11ToD3D11KeyedMutexRingChannel::EndConsume(
    const Token& token,
    ID3D11DeviceContext* consumerContext,
    bool flush) {

    CheckedSlot(token).channel.EndConsume(consumerContext, flush);
}

D3D11ToD3D11KeyedMutexRingChannel::Slot&
D3D11ToD3D11KeyedMutexRingChannel::CheckedSlot(const Token& token) {
    if (!token.IsValid()) {
        throw std::runtime_error("D3D11ToD3D11KeyedMutexRingChannel: invalid token");
    }
    if (token.slotIndex >= m_slots.size()) {
        throw std::runtime_error("D3D11ToD3D11KeyedMutexRingChannel: token slotIndex is out of range");
    }

    Slot& slot = m_slots[token.slotIndex];
    if (slot.activeSequence != token.sequence) {
        throw std::runtime_error("D3D11ToD3D11KeyedMutexRingChannel: stale token sequence");
    }
    return slot;
}

const D3D11ToD3D11KeyedMutexRingChannel::Slot&
D3D11ToD3D11KeyedMutexRingChannel::CheckedSlot(const Token& token) const {
    if (!token.IsValid()) {
        throw std::runtime_error("D3D11ToD3D11KeyedMutexRingChannel: invalid token");
    }
    if (token.slotIndex >= m_slots.size()) {
        throw std::runtime_error("D3D11ToD3D11KeyedMutexRingChannel: token slotIndex is out of range");
    }

    const Slot& slot = m_slots[token.slotIndex];
    if (slot.activeSequence != token.sequence) {
        throw std::runtime_error("D3D11ToD3D11KeyedMutexRingChannel: stale token sequence");
    }
    return slot;
}

} // namespace D3DInteropLib
