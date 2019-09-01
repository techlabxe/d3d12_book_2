#pragma once
#include "D3D12AppBase.h"
#include "DirectXMath.h"

class SampleMSAAApp : public D3D12AppBase {
public:
  SampleMSAAApp();

  virtual void Prepare();
  virtual void Cleanup();

  virtual void Render();

  struct VertexPT
  {
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT2 UV;
  };
  
  struct SceneParameter
  {
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 viewProj;
  };

  enum {
    RenderTexWidth = 512, RenderTexHeight = 512,
    SampleCount = 4,
  };

  virtual void OnSizeChanged(UINT width, UINT height, bool isMinimized);
private:
  void RenderToTexture();
  void RenderToMSAA();
  void ResolveToBackBuffer();

  void PrepareTeapot();
  void PreparePlane();
  void PrepareMsaaResource();

  using Buffer = ComPtr<ID3D12Resource1>;
  using Texture = ComPtr<ID3D12Resource1>;

  struct ModelData
  {
    UINT indexCount;
    D3D12_VERTEX_BUFFER_VIEW vbView;
    D3D12_INDEX_BUFFER_VIEW  ibView;
    Buffer resourceVB;
    Buffer resourceIB;

    ComPtr<ID3D12RootSignature> rootSig;
    ComPtr<ID3D12PipelineState> pipeline;
    std::vector<Buffer> sceneCB;
  };

  ModelData m_model;
  ModelData m_plane;

  Texture m_colorRT;
  Texture m_depthRT;
  DescriptorHandle m_hColorRTV;
  DescriptorHandle m_hDepthDSV;
  DescriptorHandle m_hColorSRV;
  DescriptorHandle m_hDepthSRV;

  Texture m_msaaColorTarget;
  Texture m_msaaDepthTarget;
  DescriptorHandle m_hMsaaRTV;
  DescriptorHandle m_hMsaaDSV;

  UINT m_frameCount;
};