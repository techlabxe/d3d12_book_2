#pragma once
#include "D3D12AppBase.h"
#include "DirectXMath.h"

class ResizableApp : public D3D12AppBase {
public:
  ResizableApp();

  virtual void Prepare();
  virtual void Cleanup();

  virtual void Render();
 
  struct SceneParameter
  {
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 viewProj;
    DirectX::XMFLOAT4 lightPos;
    DirectX::XMFLOAT4 cameraPos;
  };

  virtual void OnSizeChanged(UINT width, UINT height, bool isMinimized);
private:
  void RenderTeapot();
  void PrepareTeapot();

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
};