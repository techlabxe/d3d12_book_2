#pragma once
#include "D3D12AppBase.h"
#include <DirectXMath.h>

class InstancingApp : public D3D12AppBase {
public:
  InstancingApp();

  virtual void Prepare();
  virtual void Cleanup();

  virtual void Render();

  struct SceneParameter
  {
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 proj;
  };
private:
  using Buffer = ComPtr<ID3D12Resource1>;

  void UpdateImGui();
  void RenderImGui();
  Buffer CreateBufferResource(D3D12_HEAP_TYPE type, UINT bufferSize, D3D12_RESOURCE_STATES state);



  ComPtr<ID3DBlob> m_vs;
  ComPtr<ID3DBlob> m_ps;
  ComPtr<ID3D12RootSignature> m_rootSignature;
  ComPtr<ID3D12PipelineState> m_pipeline;

  std::vector<Buffer> m_constantBuffers;

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
    DirectX::XMFLOAT3 OffsetPos;
    DirectX::XMFLOAT4 Color;
  };

  const UINT InstanceDataMax = 200;

  ModelData m_model;
  Buffer m_instanceData;
  D3D12_VERTEX_BUFFER_VIEW m_streamView;

 
  int m_instancingCount;
  float m_cameraOffset;
};