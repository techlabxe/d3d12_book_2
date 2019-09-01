#include "PostEffectApp.h"
#include "imgui_helper.h"
#include <random>

#include "TeapotModel.h"

#include "imgui.h"
#include "examples/imgui_impl_dx12.h"
#include "examples/imgui_impl_win32.h"

using namespace DirectX;

static DirectX::XMFLOAT4 colorSet[] = {
  XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
  XMFLOAT4(1.0f, 0.65f, 1.0f, 1.0f),
  XMFLOAT4(0.1f, 0.5f, 1.0f, 1.0f),
  XMFLOAT4(0.6f, 1.0f, 0.8f, 1.0f),
};

PostEffectApp::PostEffectApp()
  : m_frameCount(0), m_effectType(EFFECT_TYPE_MOSAIC)
{
  m_effectParameter.mosaicBlockSize = 10;
  m_effectParameter.screenSize = XMFLOAT2(float(m_width), float(m_height));
  m_effectParameter.frameCount = m_frameCount;
  m_effectParameter.ripple = 0.75f;
  m_effectParameter.speed = 1.5;
  m_effectParameter.distortion = 0.03f;
  m_effectParameter.brightness = 0.25f;

}

void PostEffectApp::Prepare()
{
  SetTitle("PostEffect");
  // バッファの転送を行うためにコマンドリストを使うので準備する.
  m_commandAllocators[m_frameIndex]->Reset();
  m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr);

  PrepareTeapot();
  PreparePostEffectPlane();
  PrepareRenderTextureResource();

  // ImGui セットアップ
  auto descriptor = m_heap->Alloc();
  CD3DX12_CPU_DESCRIPTOR_HANDLE hCpu(descriptor);
  CD3DX12_GPU_DESCRIPTOR_HANDLE hGpu(descriptor);

  imgui_helper::PrepareImGui(
    m_hwnd,
    m_device.Get(),
    m_surfaceFormat,
    FrameBufferCount,
    hCpu, hGpu
  );
}
void PostEffectApp::Cleanup()
{
  imgui_helper::CleanupImGui();
}


void PostEffectApp::Render()
{
  UpdateImGui();

  m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

  m_commandAllocators[m_frameIndex]->Reset();
  m_commandList->Reset(
    m_commandAllocators[m_frameIndex].Get(),
    nullptr
  );

  ID3D12DescriptorHeap* heaps[] = { m_heap->GetHeap().Get() };
  m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

  // スワップチェイン表示可能からレンダーターゲット描画可能へ
  auto barrierToRT = m_swapchain->GetBarrierToRenderTarget();
  m_commandList->ResourceBarrier(1, &barrierToRT);

  // 通常3Dシーンをテクスチャへ描画
  RenderToTexture();

  // メインの描画先へポストエフェクト適用して描画する
  RenderToMain();

  // ImGui のフロントエンドを最後に重ねる
  RenderImGui();

  // リソースバリアを2つ設定.
  // - レンダーターゲットからスワップチェイン表示可能へ.
  // - 次回のテクスチャ描画のためにシェーダーリソースからレンダーターゲットへ.
  CD3DX12_RESOURCE_BARRIER barriers[] = {
    m_swapchain->GetBarrierToPresent(),
    CD3DX12_RESOURCE_BARRIER::Transition(
      m_colorRT.Get(), 
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET
    ),
  };
  m_commandList->ResourceBarrier(_countof(barriers), barriers);

  m_commandList->Close();

  ID3D12CommandList* lists[] = { m_commandList.Get() };

  m_commandQueue->ExecuteCommandLists(1, lists);

  m_swapchain->Present(1, 0);
  m_swapchain->WaitPreviousFrame(m_commandQueue, m_frameIndex, GpuWaitTimeout);

  m_frameCount++;
}

void PostEffectApp::OnSizeChanged(UINT width, UINT height, bool isMinimized)
{
  D3D12AppBase::OnSizeChanged(width, height, isMinimized);

  // 解像度変更のためポストエフェクト用のテクスチャを作り直す。
  if (m_colorRT)
  {
    m_heap->Free(m_hColorSRV);
    m_heapRTV->Free(m_hColorRTV);
  }
  if (m_depthRT)
  {
    m_heapDSV->Free(m_hDepthDSV);
  }
  PrepareRenderTextureResource();
}


void PostEffectApp::UpdateImGui()
{
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  auto framerate = ImGui::GetIO().Framerate;
  ImGui::Begin("Control");
  ImGui::Text("PostEffect");
  ImGui::Text("Framerate(avg) %.3f ms/frame", 1000.0f / framerate);

  ImGui::Combo("Effect", (int*)&m_effectType, "Mosaic effect\0Water effect\0\0");
  ImGui::Spacing();

  if(ImGui::CollapsingHeader("Mosaic effect", ImGuiTreeNodeFlags_DefaultOpen))
  {
    ImGui::Indent();
    ImGui::SliderFloat("Size", &m_effectParameter.mosaicBlockSize, 10, 50);
    ImGui::Unindent();
    ImGui::Spacing();
  }
  if (ImGui::CollapsingHeader("WaterEffect", ImGuiTreeNodeFlags_DefaultOpen))
  {
    ImGui::Indent();
    ImGui::SliderFloat("Ripple", &m_effectParameter.ripple, 0.1f, 1.5f);
    ImGui::SliderFloat("Speed", &m_effectParameter.speed, 1.0f, 5.0f);
    ImGui::SliderFloat("Distortion", &m_effectParameter.distortion, 0.01f, 0.5f);
    ImGui::SliderFloat("Brightness", &m_effectParameter.brightness, 0.0f, 1.0f);
    ImGui::Unindent();
    ImGui::Spacing();
  }
  ImGui::End();
}

void PostEffectApp::RenderToTexture()
{
  SceneParameter sceneParam;
  XMStoreFloat4x4(&sceneParam.view,
    XMMatrixTranspose(
      XMMatrixLookAtRH(
        XMVectorSet(3.0f, 5.0f, 5.0f, 0.0f),
        XMVectorSet(3.0f, 2.0f, 0.0f, 0.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)))
  );
  XMStoreFloat4x4(&sceneParam.proj,
    XMMatrixTranspose(
      XMMatrixPerspectiveFovRH(XMConvertToRadians(45.0f), m_viewport.Width / m_viewport.Height, 0.1f, 1000.0f)
    )
  );

  std::vector<InstanceData> instanceData(InstanceCount);
  {
    // インスタンシングデータを更新.
    for (UINT i = 0; i < InstanceCount; ++i)
    {
      float k = float( i * 20 % 360);
      float x = (i % 6) * 3.0f;
      float z = (i / 6) * -3.0f;

      auto t0 = XMMatrixTranslation(x, 0.0f, z);
      auto m0 = XMMatrixRotationZ(k);
      auto m1 = XMMatrixRotationX(k);

      auto& data = instanceData[i];
      XMStoreFloat4x4(&data.world, XMMatrixTranspose(m0 * m1 * t0));
      data.Color = colorSet[i % _countof(colorSet)];
    }
  }


  float clearColor[4] = { 0.5f,0.75f,1.0f,0 };
  // テクスチャのカラーバッファ・デプスバッファのクリア.
  m_commandList->ClearRenderTargetView(m_hColorRTV, clearColor, 0, nullptr);
  m_commandList->ClearDepthStencilView(m_hDepthDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // テクスチャを描画先にセット.
  D3D12_CPU_DESCRIPTOR_HANDLE colorDescriptors[] = { m_hColorRTV };
  D3D12_CPU_DESCRIPTOR_HANDLE depthDescirptor = m_hDepthDSV;
  m_commandList->OMSetRenderTargets(
    _countof(colorDescriptors), colorDescriptors, FALSE, &depthDescirptor);
  
  // ビューポートとシザーのセット.
  m_commandList->RSSetViewports(1, &m_viewport);
  m_commandList->RSSetScissorRects(1, &m_scissorRect);

  // 定数バッファの更新.
  void* mapped;
  auto sceneCB = m_model.sceneCB[m_frameIndex];
  if (SUCCEEDED(sceneCB->Map(0, nullptr, &mapped)))
  {
    memcpy(mapped, &sceneParam, sizeof(sceneParam));
    sceneCB->Unmap(0, nullptr);
  }
  auto instanceCB = m_model.instanceCB[m_frameIndex];
  if (SUCCEEDED(instanceCB->Map(0, nullptr, &mapped)))
  {
    memcpy(mapped, instanceData.data(), sizeof(InstanceParameter));
    instanceCB->Unmap(0, nullptr);
  }

  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_commandList->IASetVertexBuffers(0, 1, &m_model.vbView);
  m_commandList->IASetIndexBuffer(&m_model.ibView);

  m_commandList->SetGraphicsRootSignature(m_model.rootSig.Get());
  m_commandList->SetPipelineState(m_model.pipeline.Get());
  m_commandList->SetGraphicsRootConstantBufferView(
    0, sceneCB->GetGPUVirtualAddress()
  );
  m_commandList->SetGraphicsRootConstantBufferView(
    1, instanceCB->GetGPUVirtualAddress()
  );

  m_commandList->DrawIndexedInstanced(
    m_model.indexCount,
    InstanceCount,
    0, 0, 0
  );

  // 描画後、テクスチャとして使うためのバリアを設定.
  auto barrierToSRV = CD3DX12_RESOURCE_BARRIER::Transition(
    m_colorRT.Get(),
    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  );
  m_commandList->ResourceBarrier(1, &barrierToSRV);
}

void PostEffectApp::RenderToMain()
{
  // エフェクト用データの更新.
  m_effectParameter.screenSize = XMFLOAT2(float(m_width), float(m_height));
  m_effectParameter.frameCount = m_frameCount;

  auto rtv = m_swapchain->GetCurrentRTV();
  auto dsv = m_defaultDepthDSV;

  // カラーバッファ(レンダーターゲットビュー)のクリア
  float m_clearColor[4] = { 0.5f,0.75f,1.0f,0 };
  m_commandList->ClearRenderTargetView(rtv, m_clearColor, 0, nullptr);

  // デプスバッファ(デプスステンシルビュー)のクリア
  m_commandList->ClearDepthStencilView(
    dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // 描画先をセット
  m_commandList->OMSetRenderTargets(1, &(D3D12_CPU_DESCRIPTOR_HANDLE)rtv,
    FALSE, &(D3D12_CPU_DESCRIPTOR_HANDLE)dsv);

  auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(m_width), float(m_height));
  auto scissorRect = CD3DX12_RECT(0, 0, LONG(m_width), LONG(m_height));

  // ビューポートとシザーのセット
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissorRect);

  auto effectCB = m_effectCB[m_frameIndex];
  void* mapped;
  if (SUCCEEDED(effectCB->Map(0, nullptr, &mapped)))
  {
    memcpy(mapped, &m_effectParameter, sizeof(m_effectParameter));
    effectCB->Unmap(0, nullptr);
  }

  // 全画面を覆うポリゴンを描画
  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  m_commandList->SetGraphicsRootSignature(m_effectRS.Get());
  switch (m_effectType)
  {
  case EFFECT_TYPE_MOSAIC:
    m_commandList->SetPipelineState(m_mosaicPSO.Get());
    break;
  case EFFECT_TYPE_WATER:
    m_commandList->SetPipelineState(m_waterPSO.Get());
    break;
  }
  m_commandList->IASetVertexBuffers(0, 0, nullptr);
  m_commandList->SetGraphicsRootConstantBufferView(0, effectCB->GetGPUVirtualAddress());
  m_commandList->SetGraphicsRootDescriptorTable(1, m_hColorSRV);
  m_commandList->DrawInstanced(m_postEffect.vertexCount, 1, 0, 0);
}

void PostEffectApp::RenderImGui()
{
  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());
}

void PostEffectApp::PrepareTeapot()
{
  void* mapped;
  HRESULT hr;
  CD3DX12_RANGE range(0, 0);
  UINT bufferSize = sizeof(TeapotModel::TeapotVerticesPN);
  auto vbDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
  m_model.resourceVB = CreateResource(vbDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, D3D12_HEAP_TYPE_DEFAULT);
  auto uploadVB = CreateResource(vbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, D3D12_HEAP_TYPE_UPLOAD);

  hr = uploadVB->Map(0, nullptr, &mapped);
  if (SUCCEEDED(hr)) {
    memcpy(mapped, TeapotModel::TeapotVerticesPN, bufferSize);
    uploadVB->Unmap(0, nullptr);
  }
  m_model.vbView.BufferLocation = m_model.resourceVB->GetGPUVirtualAddress();
  m_model.vbView.SizeInBytes = bufferSize;
  m_model.vbView.StrideInBytes = sizeof(TeapotModel::Vertex);

  m_commandList->CopyResource(m_model.resourceVB.Get(), uploadVB.Get());

  bufferSize = sizeof(TeapotModel::TeapotIndices);
  auto ibDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
  m_model.resourceIB = CreateResource(ibDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, D3D12_HEAP_TYPE_DEFAULT);
  auto uploadIB = CreateResource(ibDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, D3D12_HEAP_TYPE_UPLOAD);

  hr = uploadIB->Map(0, nullptr, &mapped);
  if (SUCCEEDED(hr)) {
    memcpy(mapped, TeapotModel::TeapotIndices, bufferSize);
    uploadIB->Unmap(0, nullptr);
  }
  m_model.ibView.BufferLocation = m_model.resourceIB->GetGPUVirtualAddress();
  m_model.ibView.SizeInBytes = bufferSize;
  m_model.ibView.Format = DXGI_FORMAT_R32_UINT;
  m_model.indexCount = _countof(TeapotModel::TeapotIndices);

  m_commandList->CopyResource(m_model.resourceIB.Get(), uploadIB.Get());

  // コピー処理が終わった後は各バッファのステートを適切に変更しておく.
  auto barrierVB = CD3DX12_RESOURCE_BARRIER::Transition(
    m_model.resourceVB.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
  );
  auto barrierIB = CD3DX12_RESOURCE_BARRIER::Transition(
    m_model.resourceIB.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER
  );
  D3D12_RESOURCE_BARRIER barriers[] = {
    barrierVB, barrierIB,
  };
  m_commandList->ResourceBarrier(_countof(barriers), barriers);

  m_commandList->Close();
  ID3D12CommandList* command[] = { m_commandList.Get() };
  m_commandQueue->ExecuteCommandLists(1, command);

  ComPtr<ID3DBlob> vs, ps, errBlob;
  hr = CompileShaderFromFile(
    L"sceneVS.hlsl", L"vs_6_0", vs, errBlob);
  ThrowIfFailed(hr, "VertexShader error");

  hr = CompileShaderFromFile(
    L"scenePS.hlsl", L"ps_6_0", ps, errBlob);
  ThrowIfFailed(hr, "PixelShader error");

  // ルートシグネチャ構築
  CD3DX12_ROOT_PARAMETER rootParams[2];
  rootParams[0].InitAsConstantBufferView(0);
  rootParams[1].InitAsConstantBufferView(1);
  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
  rootSigDesc.Init(
    _countof(rootParams), rootParams,
    0, nullptr,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> signature;
  hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &errBlob);
  ThrowIfFailed(hr, "D3D12SerializeRootSignature failed.");
  hr = m_device->CreateRootSignature(
    0, signature->GetBufferPointer(), signature->GetBufferSize(),
    IID_PPV_ARGS(&m_model.rootSig)
  );
  ThrowIfFailed(hr, "CreateRootSignature failed.");

  // インプットレイアウト
  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(TeapotModel::Vertex, Position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, offsetof(TeapotModel::Vertex,Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
  };
  // パイプラインステートオブジェクトの生成.
  auto rasterizerDesc = book_util::CreateTeapotModelRasterizerDesc();
  auto psoDesc = book_util::CreateDefaultPsoDesc(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    vs.Get(), ps.Get(),
    rasterizerDesc, inputElementDesc, _countof(inputElementDesc),
    m_model.rootSig
  );
  hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_model.pipeline));
  ThrowIfFailed(hr, "CreateGraphicsPipelineState failed.");

  // 定数バッファの準備
  for (UINT i = 0; i < FrameBufferCount; ++i)
  {
    bufferSize = sizeof(SceneParameter);
    bufferSize = book_util::RoundupConstantBufferSize(bufferSize);
    auto cbDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    auto cb = CreateResource(cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, D3D12_HEAP_TYPE_UPLOAD);
    m_model.sceneCB.push_back(cb);

    bufferSize = sizeof(InstanceParameter);
    bufferSize = book_util::RoundupConstantBufferSize(bufferSize);
    cbDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    cb = CreateResource(cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, D3D12_HEAP_TYPE_UPLOAD);
    m_model.instanceCB.push_back(cb);
  }


  WaitForIdleGPU(); // 処理が完了するのを待つ.
}

void PostEffectApp::PreparePostEffectPlane()
{
  HRESULT hr;
  ComPtr<ID3DBlob> errBlob, mosaicVS, mosaicPS, waterVS, waterPS;
  hr = CompileShaderFromFile(L"mosaicVS.hlsl", L"vs_6_0", mosaicVS, errBlob);
  ThrowIfFailed(hr, "VertexShader error");
  hr = CompileShaderFromFile(L"mosaicPS.hlsl", L"ps_6_0", mosaicPS, errBlob);
  ThrowIfFailed(hr, "PixelShader error");
  hr = CompileShaderFromFile(L"waterVS.hlsl", L"vs_6_0", waterVS, errBlob);
  ThrowIfFailed(hr, "VertexShader error");
  hr = CompileShaderFromFile(L"waterPS.hlsl", L"ps_6_0", waterPS, errBlob);
  ThrowIfFailed(hr, "PixelShader error");

  // ルートシグネチャ構築
  CD3DX12_DESCRIPTOR_RANGE rangeSrv;
  rangeSrv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 レジスタ.
  CD3DX12_ROOT_PARAMETER rootParams[2];
  rootParams[0].InitAsConstantBufferView(0);
  rootParams[1].InitAsDescriptorTable(1, &rangeSrv, D3D12_SHADER_VISIBILITY_PIXEL);

  CD3DX12_STATIC_SAMPLER_DESC descSampler(0,
    D3D12_FILTER_MIN_MAG_MIP_LINEAR,
    D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
  rootSigDesc.Init(
    _countof(rootParams), rootParams,
    1, &descSampler,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> signature;
  hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &errBlob);
  ThrowIfFailed(hr, "D3D12SerializeRootSignature failed.");
  hr = m_device->CreateRootSignature(
    0, signature->GetBufferPointer(), signature->GetBufferSize(),
    IID_PPV_ARGS(&m_effectRS)
  );
  ThrowIfFailed(hr, "CreateRootSignature failed.");

  // インプットレイアウト
  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
  };

  // パイプラインステートオブジェクトの生成.
  auto mosaicPsoDesc = book_util::CreateDefaultPsoDesc(
    m_surfaceFormat,
    mosaicVS, mosaicPS, CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
    inputElementDesc, _countof(inputElementDesc),
    m_effectRS
  );
  hr = m_device->CreateGraphicsPipelineState(&mosaicPsoDesc, IID_PPV_ARGS(&m_mosaicPSO));
  ThrowIfFailed(hr, "CreateGraphicsPipelineState failed.(Mosaic)");

  auto waterPsoDesc = book_util::CreateDefaultPsoDesc(
    m_surfaceFormat,
    waterVS, waterPS, CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
    inputElementDesc, _countof(inputElementDesc),
    m_effectRS
  );
  hr = m_device->CreateGraphicsPipelineState(&waterPsoDesc, IID_PPV_ARGS(&m_waterPSO));
  ThrowIfFailed(hr, "CreateGraphicsPipelineState failed.(Water)");

  m_postEffect.vertexCount = 4;

  for (UINT i = 0; i < FrameBufferCount; ++i)
  {
    UINT bufferSize = sizeof(EffectParameter);
    bufferSize = book_util::RoundupConstantBufferSize(bufferSize);
    auto cbDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    auto cb = CreateResource(cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, D3D12_HEAP_TYPE_UPLOAD);
    m_effectCB.push_back(cb);
  }
}

void PostEffectApp::PrepareRenderTextureResource()
{
  auto colorTexDesc = CD3DX12_RESOURCE_DESC::Tex2D(
    DXGI_FORMAT_R8G8B8A8_UNORM, m_width, m_height,
    1, 1, 1, 0,
    D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
  );
  auto depthTexDesc = CD3DX12_RESOURCE_DESC::Tex2D(
    DXGI_FORMAT_D32_FLOAT, m_width, m_height,
    1, 1, 1, 0,
    D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
  );
  
  D3D12_CLEAR_VALUE clearColor, clearDepth;
  clearColor.Format = colorTexDesc.Format;
  clearColor.Color[0] = 0.5f;
  clearColor.Color[1] = 0.75f;
  clearColor.Color[2] = 1.0f;
  clearColor.Color[3] = 0.0f;
  clearDepth.Format = DXGI_FORMAT_D32_FLOAT;
  clearDepth.DepthStencil.Depth = 1.0f;
  clearDepth.DepthStencil.Stencil = 0;

  m_colorRT = CreateResource(colorTexDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearColor, D3D12_HEAP_TYPE_DEFAULT);
  m_depthRT = CreateResource(depthTexDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearDepth, D3D12_HEAP_TYPE_DEFAULT);

  m_hColorRTV = m_heapRTV->Alloc();
  m_device->CreateRenderTargetView(m_colorRT.Get(), nullptr, m_hColorRTV);
  m_hDepthDSV = m_heapDSV->Alloc();
  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
  dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  m_device->CreateDepthStencilView(m_depthRT.Get(), &dsvDesc, m_hDepthDSV);

  m_hColorSRV = m_heap->Alloc();
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = colorTexDesc.Format;
  srvDesc.Texture2D.MipLevels = 1;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  m_device->CreateShaderResourceView(m_colorRT.Get(), &srvDesc, m_hColorSRV);
}
