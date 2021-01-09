#include "RenderPMDApp.h"
#include "TeapotModel.h"
#include "loader/PMDloader.h"
#include "imgui_helper.h"

#include "imgui.h"
#include "examples/imgui_impl_dx12.h"
#include "examples/imgui_impl_win32.h"

#include <DirectXTex.h>
#include <fstream>

using namespace DirectX;

RenderPMDApp::RenderPMDApp()  
{
  m_camera.SetLookAt(
    XMFLOAT3(-7.0f, 14.0f, 13.0f),
    XMFLOAT3(-2.0f, 15.0f, 0.0f)
  );
}

void RenderPMDApp::Prepare()
{
  SetTitle("Render PMD file");

  m_commandList->Reset(m_commandAllocators[0].Get(), nullptr);
  ID3D12DescriptorHeap* heaps[] = { m_heap->GetHeap().Get() };
  m_commandList->SetDescriptorHeaps(1, heaps);
  m_commandList->Close();
  PrepareShadowTargets();

  // モデルファイルをロード.
  const char* filePath = "初音ミク.pmd";  // 各自で用意してください。
  m_model.Prepare(this, filePath);
  m_model.SetShadowMap(m_shadowColor.shaderAccess);
  PrepareImGui();

  m_faceWeights.resize(m_model.GetFaceMorphCount());
}

void RenderPMDApp::Cleanup()
{
  imgui_helper::CleanupImGui();
}

void RenderPMDApp::OnMouseButtonDown(UINT msg)
{
  auto io = ImGui::GetIO();
  if (io.WantCaptureMouse)
  {
    return;
  }
  m_camera.OnMouseButtonDown(msg);
}
void RenderPMDApp::OnMouseButtonUp(UINT msg)
{
  m_camera.OnMouseButtonUp();
}

void RenderPMDApp::OnMouseMove(UINT msg, int dx, int dy)
{
  auto io = ImGui::GetIO();
  if (io.WantCaptureMouse)
  {
    return;
  }
  m_camera.OnMouseMove(dx, dy);
}

// シャドウマップ描画のためのリソースの準備.
void RenderPMDApp::PrepareShadowTargets()
{
  auto colorTexDesc = CD3DX12_RESOURCE_DESC::Tex2D(
    DXGI_FORMAT_R32G32B32A32_FLOAT, ShadowSize, ShadowSize,
    1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
  auto depthTexDesc = CD3DX12_RESOURCE_DESC::Tex2D(
    DXGI_FORMAT_D32_FLOAT, ShadowSize, ShadowSize,
    1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
  D3D12_CLEAR_VALUE clearColor{}, clearDepth{};
  clearColor.Format = colorTexDesc.Format;
  clearColor.Color[0] = 1.0f; clearColor.Color[1] = 1.0f;
  clearColor.Color[2] = 1.0f; clearColor.Color[3] = 1.0f;
  clearDepth.Format = depthTexDesc.Format;
  clearDepth.DepthStencil.Depth = 1.0f;

  auto stateRenderTarget = D3D12_RESOURCE_STATE_RENDER_TARGET;
  m_shadowColor.resource = CreateResource(colorTexDesc, stateRenderTarget, &clearColor, D3D12_HEAP_TYPE_DEFAULT);
  m_shadowColor.resource->SetName(L"ShadowMap(Color)");

  auto stateDepthStencil = D3D12_RESOURCE_STATE_DEPTH_WRITE;
  m_shadowDepth.resource = CreateResource(depthTexDesc, stateDepthStencil, &clearDepth, D3D12_HEAP_TYPE_DEFAULT);
  m_shadowDepth.resource->SetName(L"ShadowMap(Depth)");

  // ディスクリプタの割り当てと書き込み(RTV).
  m_shadowColor.outputBuffer = m_heapRTV->Alloc();
  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
  rtvDesc.Format = colorTexDesc.Format;
  rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  m_device->CreateRenderTargetView(
    m_shadowColor.resource.Get(), &rtvDesc, 
    m_shadowColor.outputBuffer
  );

  // ディスクリプタの割り当てと書き込み(DSV).
  m_shadowDepth.outputBuffer = m_heapDSV->Alloc();
  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
  dsvDesc.Format = depthTexDesc.Format;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  m_device->CreateDepthStencilView(
    m_shadowDepth.resource.Get(), &dsvDesc,
    m_shadowDepth.outputBuffer);

  // 各ターゲットのシェーダー読み取り用ディスクリプタの準備.
  m_shadowColor.shaderAccess = m_heap->Alloc();
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = rtvDesc.Format;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = 1;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  m_device->CreateShaderResourceView(
    m_shadowColor.resource.Get(),
    &srvDesc,
    m_shadowColor.shaderAccess
  );
  // デプスも一応準備.
  m_shadowDepth.shaderAccess = m_heap->Alloc();
  srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
  m_device->CreateShaderResourceView(
    m_shadowDepth.resource.Get(),
    &srvDesc,
    m_shadowDepth.shaderAccess
  );
}

void RenderPMDApp::PrepareImGui()
{
  auto descriptorImGui = m_heap->Alloc();
  CD3DX12_CPU_DESCRIPTOR_HANDLE hCpu(descriptorImGui);
  CD3DX12_GPU_DESCRIPTOR_HANDLE hGpu(descriptorImGui);
  imgui_helper::PrepareImGui(
    m_hwnd,
    m_device.Get(),
    m_surfaceFormat,
    FrameBufferCount,
    hCpu, hGpu);
}

void RenderPMDApp::Render()
{
  UpdateImGui();

  if (ImGui::GetIO().MouseWheel != 0.0f && ImGui::GetIO().WantCaptureMouse == false)
  {
    int v = ImGui::GetIO().MouseWheel > 0.0f ? 120 : -120;
    //m_camera.OnZoomScroll(v);
  }

  m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
  m_commandAllocators[m_frameIndex]->Reset();
  m_commandList->Reset(
    m_commandAllocators[m_frameIndex].Get(), nullptr
  );

  // スワップチェイン表示可能からレンダーターゲット描画可能へ
  auto barrierToRT = m_swapchain->GetBarrierToRenderTarget();
  m_commandList->ResourceBarrier(1, &barrierToRT);

  ID3D12DescriptorHeap* heaps[] = { m_heap->GetHeap().Get() };
  m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

  m_scenePatameters.lightDirection = XMFLOAT4(0.0f, 20.0f,20.0f, 0.0f);
  auto mtxProj = XMMatrixPerspectiveFovRH(XMConvertToRadians(45.0f), float(m_width) / float(m_height), 0.1f, 100.0f);

  
  XMStoreFloat4x4(&m_scenePatameters.view, XMMatrixTranspose(m_camera.GetViewMatrix()));
  XMStoreFloat4x4(&m_scenePatameters.proj, XMMatrixTranspose(mtxProj));
  XMStoreFloat4(&m_scenePatameters.eyePosition, m_camera.GetPosition());

  const auto eye = XMFLOAT3(0.0f, 20.0f, 20.0f);
  const auto target = XMFLOAT3(0.0f, 0.0f, 0.0f);
  const auto up = XMFLOAT3(0.0f, 1.0f, 0.0f);
  auto shadowView = XMMatrixLookAtRH(
    XMLoadFloat3(&eye),
    XMLoadFloat3(&target),
    XMLoadFloat3(&up)
  );
  auto shadowProj = XMMatrixOrthographicRH( 20.f, 40.f, 0.1f, 100.0f );

  auto mtxLVP = shadowView * shadowProj;
  auto mtxBias = XMMatrixScaling(0.5f, -0.5f, 0.5f) * XMMatrixTranslation(0.5f, 0.5f, 0.5f);
  XMStoreFloat4x4(&m_scenePatameters.lightViewProj, XMMatrixTranspose(mtxLVP));
  XMStoreFloat4x4(&m_scenePatameters.lightViewProjBias, XMMatrixTranspose(mtxLVP*mtxBias));


  m_model.SetSceneParameter(m_scenePatameters);
  for (uint32_t i = 0; i < m_faceWeights.size(); ++i)
  {
    m_model.SetFaceMorphWeight(i, m_faceWeights[i]);
  }
  auto imageIndex = m_swapchain->GetCurrentBackBufferIndex();
  m_model.Update(imageIndex, this);


  RenderToTexture();

  // 描画後、テクスチャとして使うためのバリアを設定.
  auto barrierToSRV = CD3DX12_RESOURCE_BARRIER::Transition(
    m_shadowColor.resource.Get(),
    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  );
  m_commandList->ResourceBarrier(1, &barrierToSRV);

  // 以降バックバッファへ描画.
  RenderToMain();

  RenderImGui();

  // レンダーターゲットからスワップチェイン表示可能へ
  {
    auto barrierToPresent = m_swapchain->GetBarrierToPresent();
    auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
      m_shadowColor.resource.Get(),
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    CD3DX12_RESOURCE_BARRIER barriers[] = {
      barrierToPresent,
      barrierToRT
    };

    m_commandList->ResourceBarrier(_countof(barriers), barriers);
  }
  m_commandList->Close();
  ID3D12CommandList* lists[] = { m_commandList.Get() };
  m_commandQueue->ExecuteCommandLists(1, lists);

  m_swapchain->Present(1, 0);
  m_swapchain->WaitPreviousFrame(m_commandQueue, m_frameIndex, GpuWaitTimeout);
}



void RenderPMDApp::RenderToTexture()
{
  auto rtv = m_shadowColor.outputBuffer;
  auto dsv = m_shadowDepth.outputBuffer;

  // シャドウマップのカラーバッファ・デプスバッファのクリア.
  const float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
  m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // テクスチャを描画先にセット.
  D3D12_CPU_DESCRIPTOR_HANDLE colorDescriptors[] = { rtv };
  D3D12_CPU_DESCRIPTOR_HANDLE depthDescirptor = dsv;
  m_commandList->OMSetRenderTargets(
    _countof(colorDescriptors), colorDescriptors, FALSE, &depthDescirptor);

  // ビューポートとシザーのセット.
  auto width = ShadowSize;
  auto height = ShadowSize;
  auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(width), float(height));
  auto scissorRect = CD3DX12_RECT(0, 0, LONG(width), LONG(height));
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissorRect);

  m_model.DrawShadow(this, m_commandList);
}

void RenderPMDApp::RenderToMain()
{
  auto rtv = m_swapchain->GetCurrentRTV();
  auto dsv = m_defaultDepthDSV;

  // カラーバッファ(レンダーターゲットビュー)のクリア
  float m_clearColor[4] = { 0.5f,0.75f,1.0f,0 };
  m_commandList->ClearRenderTargetView(rtv, m_clearColor, 0, nullptr);

  // デプスバッファ(デプスステンシルビュー)のクリア
  m_commandList->ClearDepthStencilView(
    dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // 描画先をセット
  D3D12_CPU_DESCRIPTOR_HANDLE handleRtvs[] = { rtv };
  D3D12_CPU_DESCRIPTOR_HANDLE handleDsv = dsv;
  m_commandList->OMSetRenderTargets(1, handleRtvs, FALSE, &handleDsv);

  // ビューポートとシザーのセット
  auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(m_width), float(m_height));
  auto scissorRect = CD3DX12_RECT(0, 0, LONG(m_width), LONG(m_height));
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissorRect);

  m_model.Draw(this, m_commandList);
}

void RenderPMDApp::UpdateImGui()
{
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  auto framerate = ImGui::GetIO().Framerate;
  ImGui::Begin("Control");
  ImGui::Text("Framerate %.3f ms", 1000.0f / framerate);
  XMFLOAT3 cameraPos;
  XMStoreFloat3(&cameraPos, m_camera.GetPosition());
  ImGui::Text("CameraPos (%.2f, %.2f, %.2f)", cameraPos.x, cameraPos.y, cameraPos.z);
  ImGui::ColorEdit3("Outline", (float*)&m_scenePatameters.outlineColor);
  
  for (uint32_t i = 0; i < m_faceWeights.size(); ++i)
  {
    char nameBuf[64];
    sprintf_s(nameBuf, "Face %d", i);
    ImGui::SliderFloat(nameBuf, &m_faceWeights[i], 0.0f, 1.0f, "%.1f");
  }

  ImGui::End();
}

void RenderPMDApp::RenderImGui()
{
  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());
}

