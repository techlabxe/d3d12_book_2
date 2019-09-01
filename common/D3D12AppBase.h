#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d12.h>
#include <dxgi1_6.h>

#include "d3dx12.h"
#include <wrl.h>

#include "DescriptorManager.h"
#include "Swapchain.h"
#include <memory>


#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

class D3D12AppBase
{
public:
  template<class T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;

  D3D12AppBase();
  virtual ~D3D12AppBase();

  void Initialize(HWND hWnd, DXGI_FORMAT format, bool isFullScreen);
  void Terminate();

  virtual void Render();// = 0;

  virtual void Prepare() { }
  virtual void Cleanup() { }

  const UINT GpuWaitTimeout = (10 * 1000);  // 10s
  static const UINT FrameBufferCount = 2;

  virtual void OnSizeChanged(UINT width, UINT height, bool isMinimized);
  virtual void OnMouseButtonDown(UINT msg) { }
  virtual void OnMouseButtonUp(UINT msg) { }
  virtual void OnMouseMove(UINT msg, int dx, int dy) { }


  void SetTitle(const std::string& title);
  void ToggleFullscreen();

  ComPtr<ID3D12Device> GetDevice() { return m_device; }
  std::shared_ptr<Swapchain> GetSwapchain() { return m_swapchain; }

  // リソース生成
  ComPtr<ID3D12Resource1> CreateResource(
    const CD3DX12_RESOURCE_DESC& desc, 
    D3D12_RESOURCE_STATES resourceStates, 
    const D3D12_CLEAR_VALUE* clearValue,
    D3D12_HEAP_TYPE heapType
  );
  std::vector<ComPtr<ID3D12Resource1>> CreateConstantBuffers(
    const CD3DX12_RESOURCE_DESC& desc,
    int count = FrameBufferCount
  );

  // コマンドバッファ関連
  ComPtr<ID3D12GraphicsCommandList>  CreateCommandList();
  void FinishCommandList(ComPtr<ID3D12GraphicsCommandList>& command);
  ComPtr<ID3D12GraphicsCommandList> CreateBundleCommandList();

  void WriteToUploadHeapMemory(ID3D12Resource1* resource, uint32_t size, const void* pData);

  std::shared_ptr<DescriptorManager> GetDescriptorManager() { return m_heap; }
protected:

  void PrepareDescriptorHeaps();
  
  void CreateDefaultDepthBuffer(int width, int height);
  void CreateCommandAllocators();
  void WaitForIdleGPU();

  ComPtr<ID3D12Device> m_device;
  ComPtr<ID3D12CommandQueue> m_commandQueue;
 
  std::shared_ptr<Swapchain> m_swapchain;

  std::vector<ComPtr<ID3D12Resource1>> m_renderTargets;
  ComPtr<ID3D12Resource1> m_depthBuffer;

  CD3DX12_VIEWPORT  m_viewport;
  CD3DX12_RECT m_scissorRect;
  DXGI_FORMAT  m_surfaceFormat;

  
  std::vector<ComPtr<ID3D12CommandAllocator>> m_commandAllocators;
  ComPtr<ID3D12CommandAllocator> m_oneshotCommandAllocator;
  ComPtr<ID3D12CommandAllocator> m_bundleCommandAllocator;

  std::shared_ptr<DescriptorManager> m_heapRTV;
  std::shared_ptr<DescriptorManager> m_heapDSV;
  std::shared_ptr<DescriptorManager> m_heap;

  DescriptorHandle m_defaultDepthDSV;
  ComPtr<ID3D12GraphicsCommandList> m_commandList;
  HANDLE m_waitFence;

  UINT m_frameIndex;


  UINT m_width;
  UINT m_height;
  bool m_isAllowTearing;
  HWND m_hwnd;
};

HRESULT CompileShaderFromFile(
  const std::wstring& fileName, 
  const std::wstring& profile, 
  Microsoft::WRL::ComPtr<ID3DBlob>& shaderBlob, 
  Microsoft::WRL::ComPtr<ID3DBlob>& errorBlob);
