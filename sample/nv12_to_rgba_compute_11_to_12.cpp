//
// nv12_to_rgba_compute_11_to_12.cpp
// P12 sample: D3D11 allocator -> D3D12 opener, NV12 plane SRVs -> RGBA UAV.
//
#include <D3DInterop/D3DInterop.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D12Helper/D3D12Core/D3D12Barrier.hpp>
#include <D3D12Helper/D3D12Core/D3D12Core.hpp>

#include <d3dcompiler.h>

#include <cmath>
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
constexpr UINT kWidth = 64;
constexpr UINT kHeight = 32;
constexpr DXGI_FORMAT kOutputFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

const char* kNv12ToRgbaCs = R"hlsl(
Texture2D<float>  g_y  : register(t0);
Texture2D<float2> g_uv : register(t1);
RWTexture2D<float4> g_out : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    uint width;
    uint height;
    g_out.GetDimensions(width, height);
    if (tid.x >= width || tid.y >= height) {
        return;
    }

    float y = g_y.Load(int3(tid.xy, 0));
    float2 uv = g_uv.Load(int3(tid.xy / 2, 0));

    float u = uv.x - 0.5;
    float v = uv.y - 0.5;

    float3 rgb;
    rgb.r = y + 1.402000 * v;
    rgb.g = y - 0.344136 * u - 0.714136 * v;
    rgb.b = y + 1.772000 * u;

    g_out[tid.xy] = float4(saturate(rgb), 1.0);
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

bool IsSkippableD3D11FenceOrFormatError(const std::exception& e) {
    const std::string msg = e.what();
    return msg.find("ID3D11Device5") != std::string::npos ||
           msg.find("D3D11.4") != std::string::npos ||
           msg.find("OpenSharedFence") != std::string::npos ||
           msg.find("CreateTexture2D") != std::string::npos ||
           msg.find("E_INVALIDARG") != std::string::npos ||
           msg.find("HRESULT=0x80070057") != std::string::npos;
}

std::vector<std::uint8_t> MakeNeutralNv12Frame() {
    std::vector<std::uint8_t> frame(static_cast<size_t>(kWidth) * kHeight * 3u / 2u, 128u);

    // A tiny gradient in Y makes it easier to see if this sample is modified to dump the output.
    for (UINT y = 0; y < kHeight; ++y) {
        for (UINT x = 0; x < kWidth; ++x) {
            frame[static_cast<size_t>(y) * kWidth + x] = static_cast<std::uint8_t>(96u + ((x + y) % 64u));
        }
    }
    return frame;
}

D3DInteropLib::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
    ID3D12Device* device,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    UINT count,
    D3D12_DESCRIPTOR_HEAP_FLAGS flags) {

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = count;
    desc.Flags = flags;

    D3DInteropLib::ComPtr<ID3D12DescriptorHeap> heap;
    CheckHr(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)), "CreateDescriptorHeap");
    return heap;
}

D3DInteropLib::ComPtr<ID3D12Resource> CreateOutputTexture(ID3D12Device* device) {
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = kWidth;
    desc.Height = kHeight;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = kOutputFormat;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3DInteropLib::ComPtr<ID3D12Resource> texture;
    CheckHr(device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                nullptr,
                IID_PPV_ARGS(&texture)),
            "CreateCommittedResource(output RGBA UAV)");
    return texture;
}

D3DInteropLib::ComPtr<ID3D12Resource> CreateReadbackBuffer(
    ID3D12Device* device,
    UINT64 sizeBytes) {

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = sizeBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3DInteropLib::ComPtr<ID3D12Resource> buffer;
    CheckHr(device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&buffer)),
            "CreateCommittedResource(readback)");
    return buffer;
}

D3DInteropLib::ComPtr<ID3DBlob> CompileComputeShader() {
    D3DInteropLib::ComPtr<ID3DBlob> shader;
    D3DInteropLib::ComPtr<ID3DBlob> errors;

    const HRESULT hr = D3DCompile(
        kNv12ToRgbaCs,
        std::strlen(kNv12ToRgbaCs),
        "nv12_to_rgba_compute_11_to_12.hlsl",
        nullptr,
        nullptr,
        "main",
        "cs_5_0",
        0,
        0,
        &shader,
        &errors);

    if (FAILED(hr)) {
        std::string message = "D3DCompile(NV12->RGBA CS) failed";
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
    ranges[0].NumDescriptors = 2;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;

    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].RegisterSpace = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = 2;

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

void VerifyRgbaReadback(ID3D12Resource* readback,
                        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout,
                        UINT64 totalBytes) {
    void* mapped = nullptr;
    D3D12_RANGE readRange = { 0, static_cast<SIZE_T>(totalBytes) };
    CheckHr(readback->Map(0, &readRange, &mapped), "Map(readback)");

    const auto* bytes = static_cast<const std::uint8_t*>(mapped);
    bool ok = true;
    for (UINT y = 0; y < kHeight && ok; ++y) {
        const std::uint8_t* row = bytes + layout.Offset + static_cast<size_t>(y) * layout.Footprint.RowPitch;
        for (UINT x = 0; x < kWidth; ++x) {
            const std::uint8_t expected = static_cast<std::uint8_t>(96u + ((x + y) % 64u));
            const std::uint8_t r = row[x * 4u + 0u];
            const std::uint8_t g = row[x * 4u + 1u];
            const std::uint8_t b = row[x * 4u + 2u];
            const std::uint8_t a = row[x * 4u + 3u];
            const int tol = 2;
            if (std::abs(static_cast<int>(r) - static_cast<int>(expected)) > tol ||
                std::abs(static_cast<int>(g) - static_cast<int>(expected)) > tol ||
                std::abs(static_cast<int>(b) - static_cast<int>(expected)) > tol ||
                a < 250u) {
                ok = false;
                break;
            }
        }
    }

    D3D12_RANGE writtenRange = { 0, 0 };
    readback->Unmap(0, &writtenRange);
    Require(ok, "NV12->RGBA compute output does not match expected neutral-chroma luma pattern");
}

void RunSample() {
    auto core11 = D3D11CoreLib::D3D11Core::CreateShared();
    auto core12 = D3D12CoreLib::D3D12Core::CreateSharedWithAdapterLuid(core11->GetAdapterLuid());

    D3DInteropLib::SharedTextureDesc desc;
    desc.width = kWidth;
    desc.height = kHeight;
    desc.format = DXGI_FORMAT_NV12;
    desc.allowRenderTarget = false;
    desc.allowUnorderedAccess = false;
    desc.sync = D3DInteropLib::SyncPolicy::SharedFence;

    auto sharedTexture = D3DInteropLib::SharedTexture::CreateOnD3D11(*core11, desc);
    auto endpoint11 = D3DInteropLib::D3D11TextureEndpoint::Open(*core11, sharedTexture);
    auto endpoint12 = D3DInteropLib::D3D12TextureEndpoint::Open(*core12, sharedTexture);

    auto sharedFence = D3DInteropLib::SharedFence::CreateOnD3D11(*core11);
    auto fence11 = D3DInteropLib::D3D11FenceEndpoint::Open(*core11, sharedFence);
    auto fence12 = D3DInteropLib::D3D12FenceEndpoint::Open(*core12, sharedFence);

    const auto frame = MakeNeutralNv12Frame();
    core11->GetImmediateContext()->UpdateSubresource(
        endpoint11.Get(),
        0,
        nullptr,
        frame.data(),
        kWidth,
        static_cast<UINT>(frame.size()));

    fence11.Signal(core11->GetImmediateContext4(), 1);
    core11->Flush();
    fence12.CpuWait(1);

    ID3D12Device* device = core12->GetDevice();
    auto output = CreateOutputTexture(device);
    auto heap = CreateDescriptorHeap(device,
                                     D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                     3,
                                     D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    const UINT inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto cpu = heap->GetCPUDescriptorHandleForHeapStart();

    D3DInteropLib::D3D12Texture2DSrvOptions yOptions;
    yOptions.format = D3DInteropLib::GetD3DInteropVideoPlaneSrvFormat(DXGI_FORMAT_NV12, 0);
    yOptions.planeSlice = 0;
    D3DInteropLib::CreateD3D12Texture2DSrv(device, endpoint12, cpu, yOptions);

    cpu.ptr += inc;
    D3DInteropLib::D3D12Texture2DSrvOptions uvOptions;
    uvOptions.format = D3DInteropLib::GetD3DInteropVideoPlaneSrvFormat(DXGI_FORMAT_NV12, 1);
    uvOptions.planeSlice = 1;
    D3DInteropLib::CreateD3D12Texture2DSrv(device, endpoint12, cpu, uvOptions);

    cpu.ptr += inc;
    D3D12_UNORDERED_ACCESS_VIEW_DESC outputUav = {};
    outputUav.Format = kOutputFormat;
    outputUav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    outputUav.Texture2D.MipSlice = 0;
    outputUav.Texture2D.PlaneSlice = 0;
    device->CreateUnorderedAccessView(output.Get(), nullptr, &outputUav, cpu);

    const D3D12_RESOURCE_DESC outDesc = output->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
    UINT numRows = 0;
    UINT64 rowSize = 0;
    UINT64 totalBytes = 0;
    device->GetCopyableFootprints(&outDesc, 0, 1, 0, &layout, &numRows, &rowSize, &totalBytes);
    auto readback = CreateReadbackBuffer(device, totalBytes);

    auto rootSignature = CreateRootSignature(device);
    auto pso = CreateComputePso(device, rootSignature.Get());

    D3D12CoreLib::D3D12CommandContext ctx = core12->CreateDirectContext();
    ctx.Reset();

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        endpoint12.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

    ID3D12DescriptorHeap* heaps[] = { heap.Get() };
    ctx.GetCommandList()->SetDescriptorHeaps(1, heaps);
    ctx.GetCommandList()->SetComputeRootSignature(rootSignature.Get());
    ctx.GetCommandList()->SetPipelineState(pso.Get());
    ctx.GetCommandList()->SetComputeRootDescriptorTable(0, heap->GetGPUDescriptorHandleForHeapStart());
    ctx.GetCommandList()->Dispatch((kWidth + 7u) / 8u, (kHeight + 7u) / 8u, 1);

    ctx.ResourceBarrier(D3D12CoreLib::MakeUavBarrier(output.Get()));

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        endpoint12.Get(),
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_COMMON));

    ctx.ResourceBarrier(D3D12CoreLib::MakeTransitionBarrier(
        output.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_SOURCE));

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = output.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = readback.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = layout;

    ctx.GetCommandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    ctx.Close();
    ID3D12CommandList* lists[] = { ctx.GetCommandList() };
    core12->DirectQueue().ExecuteCommandLists(1, lists);
    core12->DirectQueue().WaitIdle();

    VerifyRgbaReadback(readback.Get(), layout, totalBytes);
}

} // namespace

int main() {
    try {
        RunSample();
    } catch (const std::exception& e) {
        if (IsSkippableD3D11FenceOrFormatError(e)) {
            std::cerr << "SKIP: required D3D11/D3D12 video sharing support is unavailable: "
                      << e.what() << std::endl;
            return kSkip;
        }
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Nv12ToRgbaCompute11To12 passed." << std::endl;
    return 0;
}
