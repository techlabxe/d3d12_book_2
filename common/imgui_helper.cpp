#include "imgui_helper.h"
#include "imgui.h"
#include "examples/imgui_impl_dx12.h"
#include "examples/imgui_impl_win32.h"

namespace imgui_helper
{
  void PrepareImGui(
    HWND hwnd, ComPtr<ID3D12Device> device, 
    DXGI_FORMAT formatRTV, UINT bufferCount,
    D3D12_CPU_DESCRIPTOR_HANDLE hCpu,
    D3D12_GPU_DESCRIPTOR_HANDLE hGpu
  )
  {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(hwnd);

    ImGui_ImplDX12_Init(
      device.Get(), bufferCount, formatRTV, hCpu, hGpu);
  }

  void CleanupImGui()
  {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
  }

}
