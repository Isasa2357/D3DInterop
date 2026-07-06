//
// D3D11ToD3D12TextureRingChannel.cpp
//
#include "D3D11ToD3D12TextureRingChannel.hpp"

#include <stdexcept>
#include <utility>

namespace D3DInteropLib {

D3D11ToD3D12TextureRingChannel D3D11ToD3D12TextureRingChannel::Create(
    D3D11CoreLib::D3D11Core& producer11,
    D3D12CoreLib::D3D12Core& consumer12,
    const SharedTextureDesc& desc,
    std::size_t slotCount) {

    if (slotCount == 0) {
        throw std::runtime_error("D3D11ToD3D12TextureRingChannel::Create: slotCount must be > 0");
    }

    ThrowIfAdapterMismatch(producer11.GetAdapterLuid(),
                           consumer12.GetAdapterLuid(),
                           "D3D11ToD3D12TextureRingChannel::Create");

    if (desc.sync != SyncPolicy::SharedFence) {
        throw std::runtime_error(
            "D3D11ToD3D12TextureRingChannel::Create: only SharedFence sync is supported");
    }

    D3D11ToD3D12TextureRingChannel ring;
    ring.m_slots.reserve(slotCount);
    for (std::size_t i = 0; i < slotCount; ++i) {
        Slot slot;
        slot.channel = D3D11ToD3D12TextureChannel::Create(producer11, consumer12, desc);
        slot.lastFenceValue = 0;
        ring.m_slots.push_back(std::move(slot));
    }

    ring.m_nextProducerSlot = 0;
    ring.m_nextFenceValue = 1;
    return ring;
}

D3D11ToD3D12TextureRingChannel::Token D3D11ToD3D12TextureRingChannel::BeginProduce() {
    if (m_slots.empty()) {
        throw std::runtime_error("D3D11ToD3D12TextureRingChannel::BeginProduce: channel is not initialized");
    }

    Slot& slot = m_slots[m_nextProducerSlot];
    if (slot.lastFenceValue != 0) {
        slot.channel.WaitConsumedOnProducerCpu(slot.lastFenceValue);
    }

    Token token;
    token.slotIndex = m_nextProducerSlot;
    token.fenceValue = m_nextFenceValue++;

    slot.lastFenceValue = token.fenceValue;
    m_nextProducerSlot = (m_nextProducerSlot + 1) % m_slots.size();
    return token;
}

D3D11TextureEndpoint& D3D11ToD3D12TextureRingChannel::ProducerTexture(const Token& token) {
    return CheckedSlot(token).channel.ProducerTexture();
}

const D3D11TextureEndpoint& D3D11ToD3D12TextureRingChannel::ProducerTexture(const Token& token) const {
    return CheckedSlot(token).channel.ProducerTexture();
}

void D3D11ToD3D12TextureRingChannel::EndProduce(
    ID3D11DeviceContext4* producerContext,
    const Token& token,
    bool flush) {

    CheckedSlot(token).channel.EndProduce(producerContext, token.fenceValue, flush);
}

void D3D11ToD3D12TextureRingChannel::WaitReadyOnConsumerCpu(const Token& token) {
    CheckedSlot(token).channel.WaitReadyOnConsumerCpu(token.fenceValue);
}

void D3D11ToD3D12TextureRingChannel::WaitReadyOnConsumerGpu(
    ID3D12CommandQueue* consumerQueue,
    const Token& token) {

    CheckedSlot(token).channel.WaitReadyOnConsumerGpu(consumerQueue, token.fenceValue);
}

D3D12TextureEndpoint& D3D11ToD3D12TextureRingChannel::ConsumerTexture(const Token& token) {
    return CheckedSlot(token).channel.ConsumerTexture();
}

const D3D12TextureEndpoint& D3D11ToD3D12TextureRingChannel::ConsumerTexture(const Token& token) const {
    return CheckedSlot(token).channel.ConsumerTexture();
}

void D3D11ToD3D12TextureRingChannel::EndConsume(
    ID3D12CommandQueue* consumerQueue,
    const Token& token) {

    CheckedSlot(token).channel.EndConsume(consumerQueue, token.fenceValue);
}

void D3D11ToD3D12TextureRingChannel::WaitConsumedForAllOnProducerCpu() {
    for (Slot& slot : m_slots) {
        if (slot.lastFenceValue != 0) {
            slot.channel.WaitConsumedOnProducerCpu(slot.lastFenceValue);
        }
    }
}

D3D11ToD3D12TextureRingChannel::Slot&
D3D11ToD3D12TextureRingChannel::CheckedSlot(const Token& token) {
    if (!token.IsValid()) {
        throw std::runtime_error("D3D11ToD3D12TextureRingChannel: invalid token");
    }
    if (token.slotIndex >= m_slots.size()) {
        throw std::runtime_error("D3D11ToD3D12TextureRingChannel: token slotIndex is out of range");
    }
    return m_slots[token.slotIndex];
}

const D3D11ToD3D12TextureRingChannel::Slot&
D3D11ToD3D12TextureRingChannel::CheckedSlot(const Token& token) const {
    if (!token.IsValid()) {
        throw std::runtime_error("D3D11ToD3D12TextureRingChannel: invalid token");
    }
    if (token.slotIndex >= m_slots.size()) {
        throw std::runtime_error("D3D11ToD3D12TextureRingChannel: token slotIndex is out of range");
    }
    return m_slots[token.slotIndex];
}

} // namespace D3DInteropLib
