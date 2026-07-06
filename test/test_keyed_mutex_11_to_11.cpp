//
// test_keyed_mutex_11_to_11.cpp
// Minimal D3D11 -> D3D11 shared Texture2D test using IDXGIKeyedMutex.
//
#include <D3DInterop/D3DInterop.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kSkip = 77;
constexpr UINT kWidth = 16;
constexpr UINT kHeight = 16;
constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

void CheckHr(HRESULT hr, const char* what) {
    if (FAILED(hr)) {
        std::ostringstream oss;
        oss << what << " failed: HRESULT=0x" << std::hex << static_cast<unsigned long>(hr);
        throw std::runtime_error(oss.str());
    }
}

void Require(bool cond, const char* msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

bool IsSkippableD3D11SharingError(const std::exception& e) {
    const std::string msg = e.what();
    return msg.find("OpenSharedResource1") != std::string::npos ||
           msg.find("IDXGIKeyedMutex")    != std::string::npos ||
           msg.find("KeyedMutex")         != std::string::npos;
}

std::vector<std::uint8_t> MakePattern() {
    std::vector<std::uint8_t> data(static_cast<size_t>(kWidth) * kHeight * 4u);
    for (UINT y = 0; y < kHeight; ++y) {
        for (UINT x = 0; x < kWidth; ++x) {
            const size_t i = (static_cast<size_t>(y) * kWidth + x) * 4u;
            data[i + 0] = static_cast<std::uint8_t>((x * 13u + y * 3u) & 0xffu);
            data[i + 1] = static_cast<std::uint8_t>((x * 5u  + y * 17u) & 0xffu);
            data[i + 2] = static_cast<std::uint8_t>(((x ^ y) * 11u) & 0xffu);
            data[i + 3] = 255u;
        }
    }
    return data;
}

D3DInteropLib::ComPtr<ID3D11Texture2D> CreateStagingTexture(
    ID3D11Device* device,
    UINT width,
    UINT height,
    DXGI_FORMAT format) {

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    D3DInteropLib::ComPtr<ID3D11Texture2D> texture;
    CheckHr(device->CreateTexture2D(&desc, nullptr, &texture), "CreateTexture2D(staging)");
    return texture;
}

void VerifyTextureOnD3D11(D3D11CoreLib::D3D11Core& core,
                          ID3D11Texture2D* texture,
                          const std::vector<std::uint8_t>& expected) {
    auto staging = CreateStagingTexture(core.GetDevice(), kWidth, kHeight, kFormat);
    core.GetImmediateContext()->CopyResource(staging.Get(), texture);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    CheckHr(core.GetImmediateContext()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Map(staging)");

    bool ok = true;
    const auto* bytes = static_cast<const std::uint8_t*>(mapped.pData);
    for (UINT y = 0; y < kHeight && ok; ++y) {
        const std::uint8_t* row = bytes + static_cast<size_t>(y) * mapped.RowPitch;
        const std::uint8_t* exp = expected.data() + static_cast<size_t>(y) * kWidth * 4u;
        if (std::memcmp(row, exp, kWidth * 4u) != 0) {
            ok = false;
        }
    }

    core.GetImmediateContext()->Unmap(staging.Get(), 0);
    Require(ok, "D3D11 keyed-mutex readback pixels do not match expected pattern");
}

void TestKeyedMutexD3D11ToD3D11() {
    auto producer = D3D11CoreLib::D3D11Core::CreateShared();
    auto consumer = D3D11CoreLib::D3D11Core::CreateSharedWithAdapterLuid(producer->GetAdapterLuid());

    D3DInteropLib::SharedTextureDesc desc;
    desc.width = kWidth;
    desc.height = kHeight;
    desc.format = kFormat;
    desc.allowRenderTarget = false;
    desc.allowUnorderedAccess = false;
    desc.sync = D3DInteropLib::SyncPolicy::KeyedMutex;

    auto shared = D3DInteropLib::SharedTexture::CreateOnD3D11(*producer, desc);
    auto producerEndpoint = D3DInteropLib::D3D11TextureEndpoint::Open(*producer, shared);
    auto consumerEndpoint = D3DInteropLib::D3D11TextureEndpoint::Open(*consumer, shared);

    const auto pattern = MakePattern();

    // Initial keyed mutex key is 0. Producer owns key 0, then releases key 1.
    producerEndpoint.AcquireKey(0);
    producer->GetImmediateContext()->UpdateSubresource(
        producerEndpoint.Get(),
        0,
        nullptr,
        pattern.data(),
        kWidth * 4u,
        kWidth * kHeight * 4u);
    producerEndpoint.ReleaseKey(1);

    // Consumer waits for key 1, reads, then releases key 0 so the producer could continue.
    consumerEndpoint.AcquireKey(1);
    VerifyTextureOnD3D11(*consumer, consumerEndpoint.Get(), pattern);
    consumerEndpoint.ReleaseKey(0);
}

} // namespace

int main() {
    try {
        TestKeyedMutexD3D11ToD3D11();
    } catch (const std::exception& e) {
        if (IsSkippableD3D11SharingError(e)) {
            std::cerr << "SKIP: D3D11 keyed mutex sharing is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "KeyedMutexD3D11ToD3D11 passed." << std::endl;
    return 0;
}
