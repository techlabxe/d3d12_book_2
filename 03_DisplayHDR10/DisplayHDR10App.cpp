#include "DisplayHDR10App.h"
#include "TeapotModel.h"

using namespace DirectX;

DisplayHDR10App::DisplayHDR10App()
{
}

void DisplayHDR10App::Prepare()
{
  SetTitle("DisplayHDR10");

  // バッファの転送を行うためにコマンドリストを使うので準備する.
  m_commandAllocators[m_frameIndex]->Reset();

  PrepareTeapot();
}

void DisplayHDR10App::Cleanup()
{
}

void DisplayHDR10App::Render()
{
  m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
  m_commandAllocators[m_frameIndex]->Reset();
  m_commandList->Reset(
    m_commandAllocators[m_frameIndex].Get(), nullptr
  );

  ID3D12DescriptorHeap* heaps[] = { m_heap->GetHeap().Get() };
  m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

  // スワップチェイン表示可能からレンダーターゲット描画可能へ
  auto barrierToRT = m_swapchain->GetBarrierToRenderTarget();
  m_commandList->ResourceBarrier(1, &barrierToRT);

  RenderTeapot();

  // レンダーターゲットからスワップチェイン表示可能へ
  auto barrierToPresent = m_swapchain->GetBarrierToPresent();
  m_commandList->ResourceBarrier(1, &barrierToPresent);

  m_commandList->Close();
  ID3D12CommandList* lists[] = { m_commandList.Get() };
  m_commandQueue->ExecuteCommandLists(1, lists);

  m_swapchain->Present(1, 0);
  m_swapchain->WaitPreviousFrame(m_commandQueue, m_frameIndex, GpuWaitTimeout);
}

void DisplayHDR10App::OnSizeChanged(UINT width, UINT height, bool isMinimized)
{
  D3D12AppBase::OnSizeChanged(width, height, isMinimized);
}

void DisplayHDR10App::RenderTeapot()
{
  auto rtv = m_swapchain->GetCurrentRTV();
  auto dsv = m_defaultDepthDSV;

  const float clearColor[] = {
    0.0f, 0.0f, 0.0f, 0.0f
  };
  // カラーバッファ・デプスバッファのクリア.
  m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
  m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // 描画先をセット
  m_commandList->OMSetRenderTargets(1, &(D3D12_CPU_DESCRIPTOR_HANDLE)rtv,
    FALSE, &(D3D12_CPU_DESCRIPTOR_HANDLE)dsv);

  // ビューポートとシザーのセット.
  auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(m_width), float(m_height));
  auto scissorRect = CD3DX12_RECT(0, 0, LONG(m_width), LONG(m_height));
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissorRect);

  // ティーポットの描画.
  auto cb = m_model.sceneCB[m_frameIndex];
  SceneParameter sceneParam{};
  auto mtxWorld = XMMatrixIdentity();
  auto cameraPos = XMVectorSet(0.0f, 0.0f, 5.0f, 0.0f);
  auto mtxView = XMMatrixLookAtRH(
    cameraPos,
    XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
    XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
  auto mtxProj = XMMatrixPerspectiveFovRH(XMConvertToRadians(45.0f), float(m_width) / float(m_height), 0.1f, 1000.0f);
  XMStoreFloat4x4(&sceneParam.world, XMMatrixTranspose(mtxWorld));
  XMStoreFloat4x4(&sceneParam.viewProj, XMMatrixTranspose(mtxView * mtxProj));
  XMStoreFloat4(&sceneParam.lightPos, XMVectorSet(0.0f, 10.0f, 10.0f, 0.0f));
  XMStoreFloat4(&sceneParam.cameraPos, cameraPos);
  sceneParam.branchFrags.x = m_swapchain->GetFormat() == DXGI_FORMAT_R10G10B10A2_UNORM ? 1.0f : 0.0f;

  void* mapped;
  cb->Map(0, nullptr, &mapped);
  memcpy(mapped, &sceneParam, sizeof(sceneParam));
  cb->Unmap(0, nullptr);

  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_commandList->SetGraphicsRootSignature(m_model.rootSig.Get());
  m_commandList->SetPipelineState(m_model.pipeline.Get());
  m_commandList->IASetVertexBuffers(0, 1, &m_model.vbView);
  m_commandList->IASetIndexBuffer(&m_model.ibView);
  m_commandList->SetGraphicsRootConstantBufferView(0, m_model.sceneCB[m_frameIndex]->GetGPUVirtualAddress());
  m_commandList->DrawIndexedInstanced(m_model.indexCount, 1, 0, 0, 0);
}

void DisplayHDR10App::PrepareTeapot()
{
  void* mapped;
  HRESULT hr;
  CD3DX12_RANGE range(0, 0);

  m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr);

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

  WaitForIdleGPU(); // 処理が完了するのを待つ.

  ComPtr<ID3DBlob> errBlob, vs, ps;
  hr = CompileShaderFromFile(L"modelVS.hlsl", L"vs_6_0", vs, errBlob);
  ThrowIfFailed(hr, "VertexShader error");
  hr = CompileShaderFromFile(L"modelPS.hlsl", L"ps_6_0", ps, errBlob);
  ThrowIfFailed(hr, "PixelShader error");

  // ルートシグネチャ構築
  CD3DX12_ROOT_PARAMETER rootParams[1];
  rootParams[0].InitAsConstantBufferView(0);
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

  auto surfaceFormat = m_swapchain->GetFormat();

  // パイプラインステートオブジェクトの生成.
  auto psoDesc = book_util::CreateDefaultPsoDesc(
    surfaceFormat,
    vs, ps, book_util::CreateTeapotModelRasterizerDesc(),
    inputElementDesc, _countof(inputElementDesc), m_model.rootSig);
  hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_model.pipeline));
  ThrowIfFailed(hr, "CreateGraphicsPipelineState failed.");

  // 定数バッファ準備
  bufferSize = sizeof(SceneParameter);
  bufferSize = book_util::RoundupConstantBufferSize(bufferSize);
  auto cbDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
  for (auto& cb : CreateConstantBuffers(cbDesc))
  {
    m_model.sceneCB.push_back(cb);
  }
}
