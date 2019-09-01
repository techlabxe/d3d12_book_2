#include "SampleMSAA.h"
#include "TeapotModel.h"

using namespace DirectX;

SampleMSAAApp::SampleMSAAApp()
{
  m_frameCount = 0;
}

void SampleMSAAApp::Prepare()
{
  SetTitle("RenderToTexture");

  // バッファの転送を行うためにコマンドリストを使うので準備する.
  m_commandAllocators[m_frameIndex]->Reset();

  PrepareTeapot();
  PreparePlane();

  auto width = RenderTexWidth;
  auto height = RenderTexHeight;
  // テクスチャレンダリング用
  auto colorTexDesc = CD3DX12_RESOURCE_DESC::Tex2D(
    DXGI_FORMAT_R8G8B8A8_UNORM, width, height,
    1, 1, 1, 0,
    D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
  );
  auto depthTexDesc = CD3DX12_RESOURCE_DESC::Tex2D(
    DXGI_FORMAT_D32_FLOAT, width, height,
    1, 1, 1, 0,
    D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
  );
  D3D12_CLEAR_VALUE clearColor, clearDepth;
  clearColor.Format = colorTexDesc.Format;
  clearColor.Color[0] = 1.0f;
  clearColor.Color[1] = 0.0f;
  clearColor.Color[2] = 0.0f;
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

  m_hDepthSRV = m_heap->Alloc();
  D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc{};
  depthSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
  depthSrvDesc.Texture2D.MipLevels = 1;
  depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  m_device->CreateShaderResourceView(m_depthRT.Get(), &depthSrvDesc, m_hDepthSRV);

  PrepareMsaaResource();
}

void SampleMSAAApp::Cleanup()
{
}

void SampleMSAAApp::Render()
{
  m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
  m_commandAllocators[m_frameIndex]->Reset();
  m_commandList->Reset(
    m_commandAllocators[m_frameIndex].Get(), nullptr
  );

  ID3D12DescriptorHeap* heaps[] = { m_heap->GetHeap().Get() };
  m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

  RenderToTexture();

  // MSAA バッファのステートを更新する.
  auto barrierMsaaToRT = CD3DX12_RESOURCE_BARRIER::Transition(
    m_msaaColorTarget.Get(),
    D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET
  );
  m_commandList->ResourceBarrier(1, &barrierMsaaToRT);

  RenderToMSAA();

  // MSAA 描画先からバックバッファへ内容をResolve＆転送.
  ResolveToBackBuffer();

  m_commandList->Close();
  ID3D12CommandList* lists[] = { m_commandList.Get() };
  m_commandQueue->ExecuteCommandLists(1, lists);

  m_swapchain->Present(1, 0);
  m_swapchain->WaitPreviousFrame(m_commandQueue, m_frameIndex, GpuWaitTimeout);

  m_frameCount++;
}

void SampleMSAAApp::OnSizeChanged(UINT width, UINT height, bool isMinimized)
{
  D3D12AppBase::OnSizeChanged(width, height, isMinimized);

  // MSAA描画先を作り直す.
  if (m_msaaColorTarget)
  {
    m_heapRTV->Free(m_hMsaaRTV);
  }
  if (m_msaaDepthTarget)
  {
    m_heapDSV->Free(m_hMsaaDSV);
  }
  PrepareMsaaResource();
}

void SampleMSAAApp::RenderToTexture()
{
  const float clearColor[] = {
    1.0f, 0.0f, 0.0f, 0.0f
  };
  // テクスチャのカラーバッファ・デプスバッファのクリア.
  m_commandList->ClearRenderTargetView(m_hColorRTV, clearColor, 0, nullptr);
  m_commandList->ClearDepthStencilView(m_hDepthDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // テクスチャを描画先にセット.
  D3D12_CPU_DESCRIPTOR_HANDLE colorDescriptors[] = { m_hColorRTV };
  D3D12_CPU_DESCRIPTOR_HANDLE depthDescirptor = m_hDepthDSV;
  m_commandList->OMSetRenderTargets(
    _countof(colorDescriptors), colorDescriptors, FALSE, &depthDescirptor);

  // ビューポートとシザーのセット.
  auto width = RenderTexWidth;
  auto height = RenderTexHeight;
  auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(width), float(height));
  auto scissorRect = CD3DX12_RECT(0, 0, LONG(width), LONG(height));
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissorRect);

  // ティーポットの描画.
  auto cb = m_model.sceneCB[m_frameIndex];
  SceneParameter sceneParam;
  auto mtxWorld = XMMatrixIdentity();
  auto mtxView = XMMatrixLookAtRH(
    XMVectorSet(0.0f, 2.0f, 5.0f, 0.0f),
    XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
    XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
  auto mtxProj = XMMatrixPerspectiveFovRH(XMConvertToRadians(45.0f), float(width) / float(height), 0.1f, 1000.0f);
  XMStoreFloat4x4(&sceneParam.world, XMMatrixTranspose(mtxWorld));
  XMStoreFloat4x4(&sceneParam.viewProj, XMMatrixTranspose(mtxView * mtxProj));

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

  // 描画後、テクスチャとして使うためのバリアを設定.
  auto barrierToSRV = CD3DX12_RESOURCE_BARRIER::Transition(
    m_colorRT.Get(),
    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  );
  m_commandList->ResourceBarrier(1, &barrierToSRV);
}

void SampleMSAAApp::RenderToMSAA()
{
  //auto rtv = m_swapchain->GetCurrentRTV();
  //auto dsv = m_defaultDepthDSV;

  // カラーバッファ(レンダーターゲットビュー)のクリア
  float m_clearColor[4] = { 0.0f,0.0f,0.0f,0 };
  m_commandList->ClearRenderTargetView(m_hMsaaRTV, m_clearColor, 0, nullptr);

  // デプスバッファ(デプスステンシルビュー)のクリア
  m_commandList->ClearDepthStencilView(
    m_hMsaaDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // 描画先をセット
  m_commandList->OMSetRenderTargets(1, &(D3D12_CPU_DESCRIPTOR_HANDLE)m_hMsaaRTV,
    FALSE, &(D3D12_CPU_DESCRIPTOR_HANDLE)m_hMsaaDSV);

  auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(m_width), float(m_height));
  auto scissorRect = CD3DX12_RECT(0, 0, LONG(m_width), LONG(m_height));

  // ビューポートとシザーのセット
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissorRect);

  auto mtxWorld = XMMatrixRotationY(m_frameCount*0.01f);
  auto mtxView = XMMatrixLookAtRH(
    XMVectorSet(0.0f, 0.0f, 5.0f, 0.0f),
    XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
    XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
  auto mtxProj = XMMatrixPerspectiveFovRH(XMConvertToRadians(45.0f), m_viewport.Width / m_viewport.Height, 0.1f, 1000.0f);
  {
    void* mapped;
    m_plane.sceneCB[m_frameIndex]->Map(0, nullptr, &mapped);
    auto p = static_cast<SceneParameter*>(mapped);
    XMStoreFloat4x4(&p->world, XMMatrixTranspose(mtxWorld));
    XMStoreFloat4x4(&p->viewProj, XMMatrixTranspose(mtxView * mtxProj));
    m_plane.sceneCB[m_frameIndex]->Unmap(0, nullptr);
  }

  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  m_commandList->SetGraphicsRootSignature(m_plane.rootSig.Get());
  m_commandList->SetPipelineState(m_plane.pipeline.Get());
  m_commandList->IASetVertexBuffers(0, 1, &m_plane.vbView);
  m_commandList->SetGraphicsRootConstantBufferView(0, m_plane.sceneCB[m_frameIndex]->GetGPUVirtualAddress());
  m_commandList->SetGraphicsRootDescriptorTable(1, m_hColorSRV);
  m_commandList->DrawInstanced(4, 1, 0, 0);

  // teapotを描画したテクスチャとしての使用は終わったので次回に備えてバリアをセット.
  auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
    m_colorRT.Get(),
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET
  );
  m_commandList->ResourceBarrier(1, &barrierToRT);
}

void SampleMSAAApp::ResolveToBackBuffer()
{
  // MSAAバッファを Resolve のためにステート遷移させる.
  auto toResolveSource = CD3DX12_RESOURCE_BARRIER::Transition(
    m_msaaColorTarget.Get(),
    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE
  );
  auto target = m_swapchain->GetImage(m_swapchain->GetCurrentBackBufferIndex());
  auto barrierDest = CD3DX12_RESOURCE_BARRIER::Transition(
    target.Get(),
    D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RESOLVE_DEST
  );
  D3D12_RESOURCE_BARRIER resolveBarriers[] = {
    toResolveSource, barrierDest
  };
  m_commandList->ResourceBarrier(
    UINT(_countof(resolveBarriers)), resolveBarriers
  );
  m_commandList->ResolveSubresource(
    target.Get(), 0, m_msaaColorTarget.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM
  );

  // バックバッファ（スワップチェイン）の状態を描画可能へ遷移させる.
  auto barrierPresent = CD3DX12_RESOURCE_BARRIER::Transition(
    target.Get(),
    D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT
  );
  m_commandList->ResourceBarrier(1, &barrierPresent);
}


void SampleMSAAApp::PrepareTeapot()
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

  // パイプラインステートオブジェクトの生成.
  auto psoDesc = book_util::CreateDefaultPsoDesc(
    DXGI_FORMAT_R8G8B8A8_UNORM,
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

void SampleMSAAApp::PreparePlane()
{
  VertexPT plane[] = {
    { XMFLOAT3(-1.0f, 1.0f, 0.0f), XMFLOAT2(0.0f,0.0f) },
    { XMFLOAT3( 1.0f, 1.0f, 0.0f), XMFLOAT2(1.0f,0.0f) },
    { XMFLOAT3(-1.0f,-1.0f, 0.0f), XMFLOAT2(0.0f,1.0f) },
    { XMFLOAT3( 1.0f,-1.0f, 0.0f), XMFLOAT2(1.0f,1.0f) },
  };
  void* mapped;
  HRESULT hr;

  m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr);

  UINT bufferSize = sizeof(plane);
  auto vbDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
  m_plane.resourceVB = CreateResource(vbDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, D3D12_HEAP_TYPE_DEFAULT);
  auto uploadVB = CreateResource(vbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, D3D12_HEAP_TYPE_UPLOAD);

  hr = uploadVB->Map(0, nullptr, &mapped);
  if (SUCCEEDED(hr)) {
    memcpy(mapped, plane, bufferSize);
    uploadVB->Unmap(0, nullptr);
  }
  m_plane.vbView.BufferLocation = m_plane.resourceVB->GetGPUVirtualAddress();
  m_plane.vbView.SizeInBytes = bufferSize;
  m_plane.vbView.StrideInBytes = sizeof(VertexPT);

  m_commandList->CopyResource(m_plane.resourceVB.Get(), uploadVB.Get());

  // コピー処理が終わった後は各バッファのステートを適切に変更しておく.
  auto barrierVB = CD3DX12_RESOURCE_BARRIER::Transition(
    m_plane.resourceVB.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
  );
  m_commandList->ResourceBarrier(1, &barrierVB);
  m_commandList->Close();

  ID3D12CommandList* command[] = { m_commandList.Get() };
  m_commandQueue->ExecuteCommandLists(1, command);

  WaitForIdleGPU(); // 処理が完了するのを待つ.

  ComPtr<ID3DBlob> errBlob, vs, ps;
  hr = CompileShaderFromFile(L"planeVS.hlsl", L"vs_6_0", vs, errBlob);
  ThrowIfFailed(hr, "VertexShader error");
  hr = CompileShaderFromFile(L"planePS.hlsl", L"ps_6_0", ps, errBlob);
  ThrowIfFailed(hr, "PixelShader error");

  // ルートシグネチャ構築
  CD3DX12_DESCRIPTOR_RANGE rangeSrv;
  rangeSrv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 レジスタ.
  CD3DX12_ROOT_PARAMETER rootParams[2];
  rootParams[0].InitAsConstantBufferView(0);
  rootParams[1].InitAsDescriptorTable(1, &rangeSrv, D3D12_SHADER_VISIBILITY_PIXEL);
  
  CD3DX12_STATIC_SAMPLER_DESC descSampler(0);

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
    IID_PPV_ARGS(&m_plane.rootSig)
  );
  ThrowIfFailed(hr, "CreateRootSignature failed.");

  // インプットレイアウト
  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
  };

  // パイプラインステートオブジェクトの生成.
  auto rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // 両面描画したいため.

  auto psoDesc = book_util::CreateDefaultPsoDesc(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    vs, ps, rasterizerDesc,
    inputElementDesc, _countof(inputElementDesc), m_plane.rootSig);

  // MSAA を描画先とするため SampleDesc を更新.
  psoDesc.SampleDesc.Count = SampleCount;

  hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_plane.pipeline));
  ThrowIfFailed(hr, "CreateGraphicsPipelineState failed.");

  // 定数バッファ準備
  bufferSize = sizeof(SceneParameter);
  bufferSize = book_util::RoundupConstantBufferSize(bufferSize);
  auto cbDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
  for (auto& cb : CreateConstantBuffers(cbDesc))
  {
    m_plane.sceneCB.push_back(cb);
  }
}

void SampleMSAAApp::PrepareMsaaResource()
{
  // MSAA 描画先バッファ(カラー)の準備.
  auto format = m_swapchain->GetFormat();
  auto width = m_width;
  auto height = m_height;
  D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels{};
  levels.Format = format;
  levels.SampleCount = SampleCount;

  D3D12_CLEAR_VALUE clearColor{};
  clearColor.Format = format;

  auto msaaColorDesc = CD3DX12_RESOURCE_DESC::Tex2D(
    format, width, height,
    1, 1, levels.SampleCount
  );
  msaaColorDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  auto hr = m_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
    D3D12_HEAP_FLAG_NONE,
    &msaaColorDesc,
    D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
    &clearColor,
    IID_PPV_ARGS(&m_msaaColorTarget)
  );
  ThrowIfFailed(hr, "MSAA Color バッファの生成に失敗.");

  // MSAA 描画先バッファ(デプス)の準備.
  auto msaaDepthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
    DXGI_FORMAT_D32_FLOAT, width, height,
    1, 1, SampleCount
  );
  msaaDepthDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_CLEAR_VALUE clearDepth{};
  clearDepth.Format = msaaDepthDesc.Format;
  clearDepth.DepthStencil.Depth = 1.0f;

  hr = m_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
    D3D12_HEAP_FLAG_NONE,
    &msaaDepthDesc,
    D3D12_RESOURCE_STATE_DEPTH_WRITE,
    &clearDepth,
    IID_PPV_ARGS(&m_msaaDepthTarget)
  );
  ThrowIfFailed(hr, "MSAA Depth バッファの生成に失敗.");

  // MSAA に出力するためのビューを準備する.
  D3D12_RENDER_TARGET_VIEW_DESC msaaRtvDesc{};
  msaaRtvDesc.Format = m_swapchain->GetFormat();
  msaaRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

  D3D12_DEPTH_STENCIL_VIEW_DESC msaaDsvDesc{};
  msaaDsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
  msaaDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;

  m_hMsaaRTV = m_heapRTV->Alloc();
  m_hMsaaDSV = m_heapDSV->Alloc();
  m_device->CreateRenderTargetView(m_msaaColorTarget.Get(), &msaaRtvDesc, m_hMsaaRTV);
  m_device->CreateDepthStencilView(m_msaaDepthTarget.Get(), &msaaDsvDesc, m_hMsaaDSV);
}
