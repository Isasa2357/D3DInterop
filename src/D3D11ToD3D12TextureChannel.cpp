//
// D3D11ToD3D12TextureChannel.cpp
//
#include "D3D11ToD3D12TextureChannel.hpp"

#include <stdexcept>
#include <utility>

namespace D3DInteropLib {

D3D11ToD3D12TextureChannel D3D11ToD3D12TextureChannel::Create(
    D3D11CoreLib::D3D11Core& producer11,
    D3D12CoreLib::D3D12Core& consumer12,
    const SharedTextureDesc& desc) {

    ThrowIfAdapterMismatch(producer11.GetAdapterLuid(),
                           consumer12.GetAdapterLuid(),
                           "D3D11ToD3D12TextureChannel::Create");

    if (desc.sync != SyncPolicy::SharedFence) {
        throw std::runtime_error(
            "D3D11ToD3D12TextureChannel::Create: only SharedFence sync is supported");
    }

    D3D11ToD3D12TextureChannel channel;

    channel.m_texture = SharedTexture::CreateOnD3D11(producer11, desc);
    channel.m_producerTexture11 = D3D11TextureEndpoint::Open(producer11, channel.m_texture);
    channel.m_consumerTexture12 = D3D12TextureEndpoint::Open(consumer12, channel.m_texture);

    channel.m_readyFence = SharedFence::CreateOnD3D11(producer11);
    channel.m_ready11 = D3D11FenceEndpoint::Open(producer11, channel.m_readyFence);
    channel.m_ready12 = D3D12FenceEndpoint::Open(consumer12, channel.m_readyFence);

    channel.m_consumedFence = SharedFence::CreateOnD3D12(consumer12);
    channel.m_consumed12 = D3D12FenceEndpoint::Open(consumer12, channel.m_consumedFence);
    channel.m_consumed11 = D3D11FenceEndpoint::Open(producer11, channel.m_consumedFence);

    channel.m_nextFenceValue = 1;
    return channel;
}

UINT64 D3D11ToD3D12TextureChannel::BeginProduce() {
    const UINT64 value = m_nextFenceValue++;
    if (value > 1) {
        m_consumed11.CpuWait(value - 1);
    }
    return value;
}

void D3D11ToD3D12TextureChannel::EndProduce(
    ID3D11DeviceContext4* producerContext,
    UINT64 fenceValue,
    bool flush) {

    if (!producerContext) {
        throw std::runtime_error("D3D11ToD3D12TextureChannel::EndProduce: null producerContext");
    }

    m_ready11.Signal(producerContext, fenceValue);
    if (flush) {
        producerContext->Flush();
    }
}

void D3D11ToD3D12TextureChannel::WaitReadyOnConsumerCpu(UINT64 fenceValue) {
    m_ready12.CpuWait(fenceValue);
}

void D3D11ToD3D12TextureChannel::WaitReadyOnConsumerGpu(
    ID3D12CommandQueue* consumerQueue,
    UINT64 fenceValue) {

    if (!consumerQueue) {
        throw std::runtime_error("D3D11ToD3D12TextureChannel::WaitReadyOnConsumerGpu: null consumerQueue");
    }
    m_ready12.GpuWait(consumerQueue, fenceValue);
}

void D3D11ToD3D12TextureChannel::EndConsume(
    ID3D12CommandQueue* consumerQueue,
    UINT64 fenceValue) {

    if (!consumerQueue) {
        throw std::runtime_error("D3D11ToD3D12TextureChannel::EndConsume: null consumerQueue");
    }
    m_consumed12.Signal(consumerQueue, fenceValue);
}

void D3D11ToD3D12TextureChannel::WaitConsumedOnProducerCpu(UINT64 fenceValue) {
    m_consumed11.CpuWait(fenceValue);
}

} // namespace D3DInteropLib
