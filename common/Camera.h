#pragma once
#include <DirectXMath.h>

class Camera
{
  using XMFLOAT3 = DirectX::XMFLOAT3;
  using XMFLOAT4 = DirectX::XMFLOAT4;
  using XMMATRIX = DirectX::XMMATRIX;
  using XMVECTOR = DirectX::XMVECTOR;
public:
  Camera();
  void SetLookAt(
    XMFLOAT3 vPos, XMFLOAT3 vTarget, XMFLOAT3 vUp = XMFLOAT3(0.0f,1.0f,0.0f)
  );

  void OnMouseMove(int dx, int dy);
  void OnMouseButtonDown(int buttonType);
  void OnMouseButtonUp();

  XMMATRIX GetViewMatrix() const { return m_view; }
  XMVECTOR GetPosition() const;
  
private:
  bool m_isDragged;
  int m_buttonType;
  XMMATRIX m_view;
};