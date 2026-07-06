//
// end_to_end_11_12_11.cpp
// P13 release-hardening sample:
//   D3D11 input texture -> D3D12 compute -> D3D11 output texture.
//
// Important design point:
//   Both shared textures are allocated by D3D11. D3D12 opens them and writes to
//   the output texture. This avoids the unsupported D3D12 allocator -> D3D11
//   opener quadrant while still allowing D3D12 processing results to return to
//   D3D11 renderers, encoders, or viewers.
//
#include <D3DInterop/D3DInterop.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D12Helper/D3D12Core/D3D12Barrier.hpp>
#include <D3D12Helper/D3D12Core/D3D12Core.hpp>

#include <d3dcompiler.h>

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
constexpr UINT kWidth = 32;
constexpr UINT kHeight = 32;
constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R32_UINT;
constexpr std::uint32_t kTransformMask = 0x00ff00ffu;

const char* kComputeShader = R"hlsl(
Texture2D<uint>     g_input  : register(t0);
RWTexture2D<uint>   g_output : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    uint width;
    uint height;
    g_output.GetDimensions(width, height);
    if (tid.x >= width || tid.y >= height) {
        return;
    }

    const uint value = g_input.Load(int3(tid.xy, 0));
    g_output[tid.xy] = value ^ 0x00ff00ffu;
}
)hlsl";

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

bool IsSkippableSharingError(const std::exception& e) {
    const std::string msg = e.what();
    return msg.find("ID3D11Device5") != std::string::npos ||
           msg.find("D3D11.4") != std::string::npos ||
           msg.find("OpenSharedFence") != std::string::npos ||
           msg.find("OpenSharedResource") != std::string::npos ||
           msg.find("CreateSharedHandle") != std::string::npos ||
           msg.find("CreateTexture2D") != std::string::npos ||
           msg.find("E_INVALIDARG") != std::string::npos ||
           msg.find("HRESULT=0x80070057") != std::string::npos;
}

std::vector<std::uint32_t> MakeInputPattern() {
    std::vector<std::uint32_t> data(static_cast<size_t>(kWidth) * kHeight);
    for (UINT y = 0; y < kHeight; ++y) {
        for (UINT x = 0; x < kWidth; ++x) {
            data[static_cast<size_t>(y) * kWidth + x] =
                0x10000000u |
                ((x & 0xffu) << 16u) |
                ((y & 0xffu) << 8u) |
                ((x ^ y) & 0xffu);
        }
    }
    return data;
}

std::vector<std::uint32_t> MakeExpectedOutput(const std::vector<std::uint32_t>& input) {
    std::vector<std::uint32_t> output(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] ^ kTransformMask;
    }
    return output;
}

D3DInteropLib::ComPtr<ID3D12DescriptorHeap> CreateSrvUavHeap(ID3D12Device* device, UINT count) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = count;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    desc.NodeMask = 0;

    D3DInteropLib::ComPtr<ID3D12DescriptorHeap> heap;
    CheckHr(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)), "CreateDescriptorHeap(CBV_SRV_UAV)");
    return heap;
}

D3DInteropLib::ComPtr<ID3DBlob> CompileComputeShader() {
    D3DInteropLib::ComPtr<ID3DBlob> shader;
    D3DInteropLib::ComPtr<ID3DBlob> errors;

    const HRESULT hr = D3DCompile(
        kComputeShader,
        std::strlen(kComputeShader),
        "end_to_end_11_12_11.hlsl",
        nullptr,
        nullptr,
        "main",
        "cs_5_0",
        0,
        0,
        &shader,
        &errors);

    if (FAILED(hr)) {
        std::string message = "D3DCompile(end-to-end CS) failed";
        if (errors) {
            message += ": ";
            message += static_cast<const char*>(errors->GetBufferPointer());
        }
        throw std::runtime_error(message);
    }
    return shader;
}

D3DInteropLib::ComPtr<ID3D12RootSignature> CreateRootSignature(ID3D12Device* device) {
    D3D12_DESCRIPTOR_RANGE ranges[2] = {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 1;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;

    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].RegisterSpace = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = 1;

    D3D12_ROOT_PARAMETER param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable.NumDescriptorRanges = 2;
    param.DescriptorTable.pDescriptorRanges = ranges;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 1;
    desc.pParameters = &param;
    desc.NumStaticSamplers = 0;
    desc.pStaticSamplers = nullptr;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    D3DInteropLib::ComPtr<ID3DBlob> blob;
    D3DInteropLib::ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &blob,
        &errors);
    if (FAILED(hr)) {
        std::string message = "D3D12SerializeRootSignature failed";
        if (errors) {
            message += ": ";
            message += static_cast<const char*>(errors->GetBufferPointer());
        }
        throw std::runtime_error(message);
    }

    D3DInteropLib::ComPtr<ID3D12RootSignature> rootSignature;
    CheckHr(device->CreateRootSignature(
                0,
                blob->GetBufferPointer(),
                blob->GetBufferSize(),
                IID_PPV_ARGS(&rootSignature)),
            "CreateRootSignature");
    return rootSignature;
}

D3DInteropLib::ComPtr<ID3D12PipelineState> CreateComputePso(
    ID3D12Device* device,
    ID3D12RootSignature* rootSignature) {

    auto shader = CompileComputeShader();

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = rootSignature;
    desc.CS.pShaderBytecode = shader->GetBufferPointer();
    desc.CS.BytecodeLength = shader->GetBufferSize();

    D3DInteropLib::ComPtr<ID3D12PipelineState> pso;
    CheckHr(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pso)), "CreateComputePipelineState");
    return pso;
}

D3DInteropLib::ComPtr<ID3D11Texture2D> CreateD3D11StagingReadback(
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
    CheckHr(device->CreateTexture2D(&desc, nullptr, &texture), "CreateTexture2D(D3D11 staging readback)");
    return texture;
}

void WriteInputOnD3D11(D3D11CoreLib::D3D11Core& core11,
                       ID3D11Texture2D* texture,
                       const std::vector<std::uint32_t>& input) {
    core11.GetImmediateContext()->UpdateSubresource(
        texture,
        0,
        nullptr,
        input.data(),
        kWidth * sizeof(std::uint32_t),
        kWidth * kHeight * sizeof(std::uint32_t));
}

void VerifyOutputOnD3D11(D3D11CoreLib::D3D11Core& core11,
                         ID3D11Texture2D* texture,
                         const std::vector<std::uint32_t>& expected) {
    auto staging = CreateD3D11StagingReadback(core11.GetDevice(), kWidth, kHeight, kFormat);
    core11.GetImmediateContext()->CopyResource(staging.Get(), texture);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    CheckHr(core11.GetImmediateContext()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Map(D3D11 staging readback)");

    bool ok = true;
    const auto* bytes = static_cast<const std::uint8_t*>(mapped.pData);
    for (UINT y = 0; y < kHeight && ok; ++y) {
        const auto* row = reinterpret_cast<const std::uint32_t*>(
            bytes + static_cast<size_t>(y) * mapped.RowPitch);
        const auto* exp = expected.data() + static_cast<size_t>(y) * kWidth;
        for (UINT x = 0; x < kWidth; ++x) {
            if (row[x] != exp[x]) {
                ok = false;
                break;
            }
        }
    }

    core11.GetImmediateContext()->Unmap(staging.Get(), 0);
    Require(ok, "D3D11 output texture does not match the D3D12 compute result");
}

void RecordD3D12Compute(
    D3D12CoreLib::D3D12Core& core12,
    ID3D12RootSignature* rootSignature,
    ID3D12PipelineState* pso,
    ID3D12DescriptorHeap* heap,
    D3D12_GPU_DESCRIPTOR_HANDLE tableGpu,
    ID3D12Resource* input,
    ID3D12Resource* output) {

    D3D12CoreLib::D3D12CommandContext ctx = core12.CreateDirectContext();
    ctx.Reset();

    ID3D12DescriptorHeap* heaps[] = { heap };
    ctx.GetCommandList()->SetDescriptorHeaps(1, heaps);
    ctx.GetCommandList()->SetComputeRootSignature(rootSignature);
    ctx.GetCommandList()->SetPipelineState(pso);

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        input,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        output,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    ctx.GetCommandList()->SetComputeRootDescriptorTable(0, tableGpu);
    ctx.GetCommandList()->Dispatch((kWidth + 7u) / 8u, (kHeight + 7u) / 8u, 1u);

    ctx.ResourceBarrier(D3D12CoreLib::MakeUavBarrier(output));

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        output,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON));

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        input,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_COMMON));

    ctx.Close();
    ID3D12CommandList* lists[] = { ctx.GetCommandList() };
    core12.DirectQueue().ExecuteCommandLists(1, lists);
}

void RunEndToEndSample() {
    auto core11 = D3D11CoreLib::D3D11Core::CreateShared();
    auto core12 = D3D12CoreLib::D3D12Core::CreateSharedWithAdapterLuid(core11->GetAdapterLuid());

    D3DInteropLib::SharedTextureDesc inputDesc;
    inputDesc.width = kWidth;
    inputDesc.height = kHeight;
    inputDesc.format = kFormat;
    inputDesc.allowRenderTarget = false;
    inputDesc.allowUnorderedAccess = false;
    inputDesc.sync = D3DInteropLib::SyncPolicy::SharedFence;

    D3DInteropLib::SharedTextureDesc outputDesc = inputDesc;
    outputDesc.allowUnorderedAccess = true;

    auto inputShared = D3DInteropLib::SharedTexture::CreateOnD3D11(*core11, inputDesc);
    auto outputShared = D3DInteropLib::SharedTexture::CreateOnD3D11(*core11, outputDesc);

    auto input11 = D3DInteropLib::D3D11TextureEndpoint::Open(*core11, inputShared);
    auto output11 = D3DInteropLib::D3D11TextureEndpoint::Open(*core11, outputShared);
    auto input12 = D3DInteropLib::D3D12TextureEndpoint::Open(*core12, inputShared);
    auto output12 = D3DInteropLib::D3D12TextureEndpoint::Open(*core12, outputShared);

    auto sharedFence = D3DInteropLib::SharedFence::CreateOnD3D11(*core11);
    auto fence11 = D3DInteropLib::D3D11FenceEndpoint::Open(*core11, sharedFence);
    auto fence12 = D3DInteropLib::D3D12FenceEndpoint::Open(*core12, sharedFence);

    const auto input = MakeInputPattern();
    const auto expected = MakeExpectedOutput(input);
    WriteInputOnD3D11(*core11, input11.Get(), input);

    fence11.Signal(core11->GetImmediateContext4(), 1);
    core11->Flush();

    auto heap = CreateSrvUavHeap(core12->GetDevice(), 2);
    const UINT increment = core12->GetDevice()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = heap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE uavCpu = srvCpu;
    uavCpu.ptr += increment;

    D3D12_GPU_DESCRIPTOR_HANDLE tableGpu = heap->GetGPUDescriptorHandleForHeapStart();

    D3DInteropLib::D3D12Texture2DSrvOptions srvOptions;
    srvOptions.format = kFormat;
    srvOptions.mostDetailedMip = 0;
    srvOptions.mipLevels = 1;
    srvOptions.planeSlice = 0;

    D3DInteropLib::D3D12Texture2DUavOptions uavOptions;
    uavOptions.format = kFormat;
    uavOptions.mipSlice = 0;
    uavOptions.planeSlice = 0;

    D3DInteropLib::CreateD3D12Texture2DSrv(core12->GetDevice(), input12, srvCpu, srvOptions);
    D3DInteropLib::CreateD3D12Texture2DUav(core12->GetDevice(), output12, uavCpu, uavOptions);

    auto rootSignature = CreateRootSignature(core12->GetDevice());
    auto pso = CreateComputePso(core12->GetDevice(), rootSignature.Get());

    // Keep the wait on the D3D12 GPU timeline. The command list submitted below is
    // ordered after the D3D11 producer's signal.
    fence12.GpuWait(core12->GetDirectCommandQueue(), 1);

    RecordD3D12Compute(
        *core12,
        rootSignature.Get(),
        pso.Get(),
        heap.Get(),
        tableGpu,
        input12.Get(),
        output12.Get());

    fence12.Signal(core12->GetDirectCommandQueue(), 2);
    fence11.CpuWait(2);

    VerifyOutputOnD3D11(*core11, output11.Get(), expected);
    core12->DirectQueue().WaitIdle();
}

} // namespace

int main() {
    try {
        RunEndToEndSample();
    } catch (const std::exception& e) {
        if (IsSkippableSharingError(e)) {
            std::cerr << "SKIP: required D3D11/D3D12 sharing support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "EndToEnd11To12To11 passed." << std::endl;
    return 0;
}
