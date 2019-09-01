#pragma once

#pragma once
#include "D3D12AppBase.h"
#include <DirectXMath.h>

class BundleApp : public D3D12AppBase {
public:
  BundleApp();

  virtual void Prepare();
  virtual void Cleanup();

  virtual void Render();

  struct SceneParameter
  {
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 proj;
  };
private:
  using Buffer = ComPtr<ID3D12Resource1>;

  Buffer CreateBufferResource(D3D12_HEAP_TYPE type, UINT bufferSize, D3D12_RESOURCE_STATES state);



  ComPtr<ID3DBlob> m_vs;
  ComPtr<ID3DBlob> m_ps;
  ComPtr<ID3D12RootSignature> m_rootSignature;
  ComPtr<ID3D12PipelineState> m_pipeline;

  std::vector<Buffer> m_constantBuffers;

  enum {
    InstanceDataMax = 500,
  };

  struct ModelData
  {
    UINT indexCount;
    D3D12_VERTEX_BUFFER_VIEW vbView;
    D3D12_INDEX_BUFFER_VIEW  ibView;
    Buffer resourceVB;
    Buffer resourceIB;
  };

  struct InstanceData
  {
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4 Color;
  };
  struct InstanceParameter
  {
    InstanceData  data[InstanceDataMax];
  };

  ModelData m_model;
  std::vector<Buffer> m_instanceBuffers;

  int m_instancingCount;
  
  ComPtr<ID3D12CommandAllocator> m_bundleAllocator;
  std::vector<ComPtr<ID3D12GraphicsCommandList>> m_bundles;
};