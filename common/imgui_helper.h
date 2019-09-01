#pragma once
#include <d3d12.h>
#include <wrl.h>

namespace imgui_helper
{
  template<class T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;

  void PrepareImGui(
    HWND hwnd, ComPtr<ID3D12Device> device,
    DXGI_FORMAT formatRTV, UINT bufferCount,
    D3D12_CPU_DESCRIPTOR_HANDLE hCpu,
    D3D12_GPU_DESCRIPTOR_HANDLE hGpu
    );
  void CleanupImGui();

}
