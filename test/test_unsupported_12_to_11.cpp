//
// test_unsupported_12_to_11.cpp
// D3D12 allocator -> D3D11 opener must be rejected at D3DInterop API level.
//
#include <D3DInterop/D3DInterop.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D12Helper/D3D12Core/D3D12Core.hpp>

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr int kSkip = 77;

bool IsSkippableDeviceError(const std::exception& e) {
    const std::string msg = e.what();
    return msg.find("D3D12") != std::string::npos &&
           (msg.find("not available") != std::string::npos ||
            msg.find("feature") != std::string::npos);
}

void Require(bool cond, const char* msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

void TestUnsupportedD3D12ToD3D11Open() {
    auto core12 = D3D12CoreLib::D3D12Core::CreateShared();
    auto core11 = D3D11CoreLib::D3D11Core::CreateSharedWithAdapterLuid(core12->GetAdapterLuid());

    D3DInteropLib::SharedTextureDesc desc;
    desc.width = 8;
    desc.height = 8;
    desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.allowRenderTarget = false;
    desc.allowUnorderedAccess = false;
    desc.sync = D3DInteropLib::SyncPolicy::SharedFence;

    auto shared = D3DInteropLib::SharedTexture::CreateOnD3D12(*core12, desc);

    bool rejected = false;
    try {
        auto endpoint11 = D3DInteropLib::D3D11TextureEndpoint::Open(*core11, shared);
        (void)endpoint11;
    } catch (const std::runtime_error& e) {
        const std::string msg = e.what();
        rejected =
            msg.find("unsupported texture quadrant") != std::string::npos &&
            msg.find("D3D12 allocator -> D3D11 opener") != std::string::npos;
    }

    Require(rejected,
            "D3D12 allocator -> D3D11 opener was not rejected at the D3DInterop API level");
}

} // namespace

int main() {
    try {
        TestUnsupportedD3D12ToD3D11Open();
    } catch (const std::exception& e) {
        if (IsSkippableDeviceError(e)) {
            std::cerr << "SKIP: required Direct3D device support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "UnsupportedD3D12ToD3D11 passed." << std::endl;
    return 0;
}
