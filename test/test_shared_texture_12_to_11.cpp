//
// test_shared_texture_12_to_11.cpp
// T1: D3D12 allocator -> D3D11 opener shared texture roundtrip.
//
#include <D3DInterop/D3DInterop.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D12Helper/D3D12Core/D3D12Barrier.hpp>
#include <D3D12Helper/D3D12Core/D3D12Core.hpp>
#include <D3D12Helper/D3D12Framework/D3D12Helpers.hpp>
#include <D3D12Helper/D3D12Framework/D3D12Resource.hpp>
#include <D3D12Helper/D3D12Framework/D3D12UploadBuffer.hpp>

#include <cstdint>
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

std::vector<std::uint8_t> MakePattern(UINT width, UINT height) {
    std::vector<std::uint8_t> data(static_cast<size_t>(width) * height * 4u);
    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            const size_t i = (static_cast<size_t>(y) * width + x) * 4u;
            data[i + 0] = static_cast<std::uint8_t>((x * 13u + y * 3u) & 0xffu);
            data[i + 1] = static_cast<std::uint8_t>((x * 5u  + y * 17u) & 0xffu);
            data[i + 2] = static_cast<std::uint8_t>((x ^ y) * 11u & 0xffu);
            data[i + 3] = 255u;
        }
    }
    return data;
}

bool IsSkippableD3D11FenceError(const std::exception& e) {
    const std::string msg = e.what();
    return msg.find("ID3D11Device5") != std::string::npos ||
           msg.find("D3D11.4")       != std::string::npos ||
           msg.find("OpenSharedFence") != std::string::npos;
}

void UploadPatternOnD3D12(D3D12CoreLib::D3D12Core& core12,
                          D3DInteropLib::D3D12TextureEndpoint& endpoint12,
                          const std::vector<std::uint8_t>& pattern) {
    D3D12CoreLib::D3D12Resource texture(endpoint12.Resource(), D3D12_RESOURCE_STATE_COMMON);

    const UINT64 uploadBytes = D3D12CoreLib::GetRequiredUploadSize(core12, texture);
    D3D12CoreLib::D3D12UploadBuffer upload;
    upload.Initialize(core12.GetDevice(), uploadBytes);

    D3D12CoreLib::D3D12CommandContext ctx = core12.CreateDirectContext();
    ctx.Reset();

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        texture.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST));
    texture.SetState(D3D12_RESOURCE_STATE_COPY_DEST);

    D3D12CoreLib::RecordUploadTexture2D(
        core12,
        ctx,
        texture,
        upload,
        pattern.data(),
        kWidth,
        kHeight,
        kFormat,
        kWidth * 4u,
        D3D12_RESOURCE_STATE_COMMON);

    ctx.Close();
    ID3D12CommandList* lists[] = { ctx.GetCommandList() };
    core12.DirectQueue().ExecuteCommandLists(1, lists);
}

void VerifyPatternOnD3D11(D3D11CoreLib::D3D11Core& core11,
                          D3DInteropLib::D3D11TextureEndpoint& endpoint11,
                          const std::vector<std::uint8_t>& expected) {
    D3D11_TEXTURE2D_DESC desc = {};
    endpoint11.Get()->GetDesc(&desc);
    Require(desc.Width == kWidth && desc.Height == kHeight, "D3D11 opened texture has unexpected size");
    Require(desc.Format == kFormat, "D3D11 opened texture has unexpected format");

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    D3DInteropLib::ComPtr<ID3D11Texture2D> staging;
    CheckHr(core11.GetDevice()->CreateTexture2D(&stagingDesc, nullptr, &staging),
            "CreateTexture2D(staging)");

    ID3D11DeviceContext* ctx = core11.GetImmediateContext();
    ctx->CopyResource(staging.Get(), endpoint11.Get());
    core11.Flush();

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    CheckHr(ctx->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Map(staging)");

    const auto* src = static_cast<const std::uint8_t*>(mapped.pData);
    bool ok = true;
    for (UINT y = 0; y < kHeight && ok; ++y) {
        const std::uint8_t* row = src + static_cast<size_t>(y) * mapped.RowPitch;
        const std::uint8_t* exp = expected.data() + static_cast<size_t>(y) * kWidth * 4u;
        for (UINT x = 0; x < kWidth * 4u; ++x) {
            if (row[x] != exp[x]) {
                ok = false;
                break;
            }
        }
    }

    ctx->Unmap(staging.Get(), 0);
    Require(ok, "D3D11 readback pixels do not match D3D12 uploaded pattern");
}

void TestD3D12ToD3D11SharedTexture() {
    auto core12 = D3D12CoreLib::D3D12Core::CreateShared();
    auto core11 = D3D11CoreLib::D3D11Core::CreateSharedWithAdapterLuid(core12->GetAdapterLuid());

    D3DInteropLib::SharedTextureDesc desc;
    desc.width = kWidth;
    desc.height = kHeight;
    desc.format = kFormat;
    desc.allowRenderTarget = false;
    desc.allowUnorderedAccess = false;
    desc.sync = D3DInteropLib::SyncPolicy::SharedFence;

    auto sharedTexture = D3DInteropLib::SharedTexture::CreateOnD3D12(*core12, desc);
    auto endpoint12 = D3DInteropLib::D3D12TextureEndpoint::Open(*core12, sharedTexture);
    auto endpoint11 = D3DInteropLib::D3D11TextureEndpoint::Open(*core11, sharedTexture);

    auto sharedFence = D3DInteropLib::SharedFence::CreateOnD3D12(*core12);
    auto fence12 = D3DInteropLib::D3D12FenceEndpoint::Open(*core12, sharedFence);
    auto fence11 = D3DInteropLib::D3D11FenceEndpoint::Open(*core11, sharedFence);

    const std::vector<std::uint8_t> pattern = MakePattern(kWidth, kHeight);
    UploadPatternOnD3D12(*core12, endpoint12, pattern);

    fence12.Signal(core12->GetDirectCommandQueue(), 1);
    fence11.CpuWait(1);

    VerifyPatternOnD3D11(*core11, endpoint11, pattern);
}

} // namespace

int main() {
    try {
        TestD3D12ToD3D11SharedTexture();
    } catch (const std::exception& e) {
        if (IsSkippableD3D11FenceError(e)) {
            std::cerr << "SKIP: required D3D11.4 shared fence support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "SharedTexture12To11 passed." << std::endl;
    return 0;
}
