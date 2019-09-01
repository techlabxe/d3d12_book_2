#pragma once
#include "D3D12AppBase.h"

class SampleImGui : public D3D12AppBase {
public:
  virtual void Prepare();
  virtual void Cleanup();

  virtual void Render();
private:
  void UpdateImGui();
  void RenderImGui();

  float m_factor;
  float m_clearColor[4];
};