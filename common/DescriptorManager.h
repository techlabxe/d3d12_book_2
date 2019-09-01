#pragma once
#include <wrl.h>
#include <list>

#include "D3D12BookUtil.h"
#include "d3dx12.h"

class DescriptorHandle
{
public:
  DescriptorHandle() : m_handleCpu(), m_handleGpu() {}

  DescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE hCpu, D3D12_GPU_DESCRIPTOR_HANDLE hGpu)
    : m_handleCpu(hCpu), m_handleGpu(hGpu)
  {
  }

  operator D3D12_CPU_DESCRIPTOR_HANDLE() const { return m_handleCpu; }
  operator D3D12_GPU_DESCRIPTOR_HANDLE() const { return m_handleGpu; }

private:
  D3D12_CPU_DESCRIPTOR_HANDLE m_handleCpu;
  D3D12_GPU_DESCRIPTOR_HANDLE m_handleGpu;
};

class DescriptorManager
{
public:
  template<class T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;

  DescriptorManager(ComPtr<ID3D12Device> device, const D3D12_DESCRIPTOR_HEAP_DESC& desc)
    : m_index(0), m_incrementSize(0)
  {
    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap));
    ThrowIfFailed(hr, "CreateDescriptorHeap ‚ÉŽ¸”s.");

    m_handleCpu = m_heap->GetCPUDescriptorHandleForHeapStart();
    m_handleGpu = m_heap->GetGPUDescriptorHandleForHeapStart();
    m_incrementSize = device->GetDescriptorHandleIncrementSize(desc.Type);
  }
  ComPtr<ID3D12DescriptorHeap> GetHeap() const { return m_heap; }

  DescriptorHandle Alloc()
  {
    if (!m_freeList.empty())
    {
      auto ret = m_freeList.front();
      m_freeList.pop_front();
      return ret;
    }

    UINT use = m_index++;
    auto ret = DescriptorHandle(
      m_handleCpu.Offset(use, m_incrementSize),
      m_handleGpu.Offset(use, m_incrementSize)
    );

    return ret;
  }

  void Free(const DescriptorHandle& handle)
  {
    m_freeList.push_back(handle);
  }


private:
  ComPtr<ID3D12DescriptorHeap> m_heap;
  CD3DX12_CPU_DESCRIPTOR_HANDLE m_handleCpu;
  CD3DX12_GPU_DESCRIPTOR_HANDLE m_handleGpu;
  UINT m_index;
  UINT m_incrementSize;

  std::list<DescriptorHandle> m_freeList;
};