#pragma once
#include "D3D12AppBase.h"
#include "DirectXMath.h"
#include "Camera.h"

#include "Model.h"
#include "Animator.h"

class AnimationApp : public D3D12AppBase {
public:
  AnimationApp();

  virtual void Prepare();
  virtual void Cleanup();

  virtual void Render();

  virtual void OnMouseButtonDown(UINT msg);
  virtual void OnMouseButtonUp(UINT msg);
  virtual void OnMouseMove(UINT msg, int dx, int dy);
private:
  void PrepareShadowTargets();
  void PrepareImGui();
  void UpdateImGui();
  void RenderToTexture();
  void RenderToMain();
  void RenderImGui();

  using Buffer = ComPtr<ID3D12Resource1>;
  using Texture = ComPtr<ID3D12Resource1>;
  

  enum
  {
    ShadowSize = 1024,
  };


  UINT m_frameCount;
  Camera m_camera;

  Model m_model;
  Model::SceneParameter m_scenePatameters;

  struct RenderTarget
  {
    ComPtr<ID3D12Resource1> resource;
    DescriptorHandle  outputBuffer;
    DescriptorHandle  shaderAccess;
  };
  RenderTarget m_shadowColor, m_shadowDepth;

  Animator m_animator;
  bool m_isAnimeStart;
};