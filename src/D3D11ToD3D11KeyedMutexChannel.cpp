//
// D3D11ToD3D11KeyedMutexChannel.cpp
//
#include "D3D11ToD3D11KeyedMutexChannel.hpp"

#include <stdexcept>

namespace D3DInteropLib {

D3D11ToD3D11KeyedMutexChannel D3D11ToD3D11KeyedMutexChannel::Create(
    D3D11CoreLib::D3D11Core& producer11,
    D3D11CoreLib::D3D11Core& consumer11,
    const SharedTextureDesc& desc) {

    ThrowIfAdapterMismatch(producer11.GetAdapterLuid(),
                           consumer11.GetAdapterLuid(),
                           "D3D11ToD3D11KeyedMutexChannel::Create");

    if (desc.sync != SyncPolicy::KeyedMutex) {
        throw std::runtime_error(
            "D3D11ToD3D11KeyedMutexChannel::Create: only KeyedMutex sync is supported");
    }

    D3D11ToD3D11KeyedMutexChannel channel;
    channel.m_texture = SharedTexture::CreateOnD3D11(producer11, desc);
    channel.m_producerTexture11 = D3D11TextureEndpoint::Open(producer11, channel.m_texture);
    channel.m_consumerTexture11 = D3D11TextureEndpoint::Open(consumer11, channel.m_texture);
    return channel;
}

void D3D11ToD3D11KeyedMutexChannel::BeginProduce(DWORD timeoutMs) {
    m_producerTexture11.AcquireKey(kProducerKey, timeoutMs);
}

void D3D11ToD3D11KeyedMutexChannel::EndProduce(
    ID3D11DeviceContext* producerContext,
    bool flush) {

    if (flush) {
        if (!producerContext) {
            throw std::runtime_error(
                "D3D11ToD3D11KeyedMutexChannel::EndProduce: flush requested but producerContext is null");
        }
        producerContext->Flush();
    }

    m_producerTexture11.ReleaseKey(kConsumerKey);
}

void D3D11ToD3D11KeyedMutexChannel::BeginConsume(DWORD timeoutMs) {
    m_consumerTexture11.AcquireKey(kConsumerKey, timeoutMs);
}

void D3D11ToD3D11KeyedMutexChannel::EndConsume(
    ID3D11DeviceContext* consumerContext,
    bool flush) {

    if (flush) {
        if (!consumerContext) {
            throw std::runtime_error(
                "D3D11ToD3D11KeyedMutexChannel::EndConsume: flush requested but consumerContext is null");
        }
        consumerContext->Flush();
    }

    m_consumerTexture11.ReleaseKey(kProducerKey);
}

} // namespace D3DInteropLib
