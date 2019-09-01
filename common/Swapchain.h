#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>

#include <wrl.h>
#include <vector>
#include <memory>

#include "DescriptorManager.h"
#include "D3D12BookUtil.h"

class Swapchain
{
public:
  template<class T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;

  Swapchain(
    ComPtr<IDXGISwapChain1> swapchain,
    std::shared_ptr<DescriptorManager>& heapRTV,
    bool useHDR = false);

  ~Swapchain();

  UINT GetCurrentBackBufferIndex() const {
    return m_swapchain->GetCurrentBackBufferIndex();
  }
  DescriptorHandle GetCurrentRTV() const;
  ComPtr<ID3D12Resource1> GetImage(UINT index) { return m_images[index]; }

  HRESULT Present(UINT SyncInterval, UINT Flags);

  // 次のコマンドが積めるようになるまで待機.
  void WaitPreviousFrame(
    ComPtr<ID3D12CommandQueue> commandQueue, 
    int frameIndex, DWORD timeout);

  void ResizeBuffers(UINT width, UINT height);

  // 現在のイメージに対して描画可能バリア設定の取得.
  CD3DX12_RESOURCE_BARRIER GetBarrierToRenderTarget();
  // 現在のイメージに対して表示可能バリア設定の取得.
  CD3DX12_RESOURCE_BARRIER GetBarrierToPresent();

  DXGI_FORMAT GetFormat() const { return m_desc.Format; }

  bool IsFullScreen() const;
  void SetFullScreen(bool toFullScreen);
  void ResizeTarget(const DXGI_MODE_DESC *pNewTargetParameters);
private:
  // メタデータのセット.
  void SetMetadata();

private:
  ComPtr<IDXGISwapChain4> m_swapchain;
  std::vector<ComPtr<ID3D12Resource1>> m_images;
  std::vector<DescriptorHandle> m_imageRTV;

  std::vector<UINT64> m_fenceValues;
  std::vector<ComPtr<ID3D12Fence1>> m_fences;

  DXGI_SWAP_CHAIN_DESC1 m_desc;

  HANDLE m_waitEvent;
};