#include <D3DInterop/D3DInterop.hpp>

#include <iostream>

int main() {
    D3DInteropLib::SharedTextureDesc desc;
    desc.width = 16;
    desc.height = 16;
    desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.sync = D3DInteropLib::SyncPolicy::SharedFence;

    if (desc.width != 16 || desc.height != 16) {
        return 1;
    }

    std::cout << "D3DInterop package consumer passed." << std::endl;
    return 0;
}
