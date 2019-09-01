#pragma once
#include "D3D12AppBase.h"
#include <DirectXMath.h>

class PostEffectApp : public D3D12AppBase {
public:
  PostEffectApp();

  virtual void Prepare();
  virtual void Cleanup();

  virtual void Render();
  virtual void OnSizeChanged(UINT width, UINT height, bool isMinimized);

  struct SceneParameter
  {
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 proj;
  };

  struct EffectParameter
  {
    DirectX::XMFLOAT2  screenSize;
    float mosaicBlockSize;

    UINT  frameCount;
    float ripple;
    float speed; // ó¨ÇÍë¨ìx.
    float  distortion; // òcÇ›ã≠ìx
    float  brightness; // ñæÇÈÇ≥åWêî
  };
private:
  using Buffer = ComPtr<ID3D12Resource1>;
  using Texture = ComPtr<ID3D12Resource1>;

  void RenderToTexture();
  void RenderToMain();

  void UpdateImGui();
  void RenderImGui();

  void PrepareTeapot();
  void PreparePostEffectPlane();
  void PrepareRenderTextureResource();

  enum {
    InstanceCount = 200,
  };
  enum EffectType
  {
    EFFECT_TYPE_MOSAIC,
    EFFECT_TYPE_WATER,
  };

  struct InstanceData
  {
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4 Color;
  };
  struct InstanceParameter
  {
    InstanceData  data[InstanceCount];
  };

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
    std::vector<Buffer> instanceCB;
  };
  struct PlaneData
  {
    UINT vertexCount;
    D3D12_VERTEX_BUFFER_VIEW vbView;
    Buffer resourceVB;
  };

  Texture m_colorRT, m_depthRT;
  DescriptorHandle m_hColorRTV;
  DescriptorHandle m_hDepthDSV;
  DescriptorHandle m_hColorSRV;

  ModelData m_model;
  PlaneData m_postEffect;

  UINT m_frameCount;

  EffectParameter m_effectParameter;
  std::vector<Buffer> m_effectCB;

  EffectType m_effectType;
  ComPtr<ID3D12RootSignature> m_effectRS;
  ComPtr<ID3D12PipelineState> m_mosaicPSO, m_waterPSO;
};