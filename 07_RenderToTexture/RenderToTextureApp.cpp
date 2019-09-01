#include "RenderToTextureApp.h"
#include "TeapotModel.h"

using namespace DirectX;

RenderToTextureApp::RenderToTextureApp()
{
  m_frameCount = 0;
}

void RenderToTextureApp::Prepare()
{
  SetTitle("RenderToTexture");

  // �o�b�t�@�̓]�����s�����߂ɃR�}���h���X�g���g���̂ŏ�������.
  m_commandAllocators[m_frameIndex]->Reset();

  PrepareTeapot();
  PreparePlane();

  auto width = RenderTexWidth;
  auto height = RenderTexHeight;
  // �e�N�X�`�������_�����O�p
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
  clearColor.Color[0] = 0.25f;
  clearColor.Color[1] = 0.25f;
  clearColor.Color[2] = 0.25f;
  clearColor.Color[3] = 0.0f;
  clearDepth.Format = DXGI_FORMAT_D32_FLOAT;
  clearDepth.DepthStencil.Depth = 1.0f;
  clearDepth.DepthStencil.Stencil = 0;

  HRESULT hr;
  hr = m_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
    D3D12_HEAP_FLAG_NONE,
    &colorTexDesc,
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    &clearColor,
    IID_PPV_ARGS(&m_colorRT)
  );
  ThrowIfFailed(hr, "CreateCommittedResource(Color)");
  hr = m_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
    D3D12_HEAP_FLAG_NONE,
    &depthTexDesc,
    D3D12_RESOURCE_STATE_DEPTH_WRITE,
    &clearDepth,
    IID_PPV_ARGS(&m_depthRT)
  );
  ThrowIfFailed(hr, "CreateCommittedResource(Depth)");

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

}

void RenderToTextureApp::Cleanup()
{
}

void RenderToTextureApp::Render()
{
  m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
  m_commandAllocators[m_frameIndex]->Reset();
  m_commandList->Reset(
    m_commandAllocators[m_frameIndex].Get(), nullptr
  );

  // �X���b�v�`�F�C���\���\���烌���_�[�^�[�Q�b�g�`��\��
  auto barrierToRT = m_swapchain->GetBarrierToRenderTarget();
  m_commandList->ResourceBarrier(1, &barrierToRT);

  ID3D12DescriptorHeap* heaps[] = { m_heap->GetHeap().Get() };
  m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

  RenderToTexture();

  RenderToMain();

  m_commandList->Close();
  ID3D12CommandList* lists[] = { m_commandList.Get() };
  m_commandQueue->ExecuteCommandLists(1, lists);

  m_swapchain->Present(1, 0);
  m_swapchain->WaitPreviousFrame(m_commandQueue, m_frameIndex, GpuWaitTimeout);

  m_frameCount++;
}

void RenderToTextureApp::OnSizeChanged(UINT width, UINT height, bool isMinimized)
{
  D3D12AppBase::OnSizeChanged(width, height, isMinimized);
}

void RenderToTextureApp::RenderToTexture()
{
  const float clearColor[] = {
    0.25f, 0.25f, 0.25f, 0.0f
  };
  // �e�N�X�`���̃J���[�o�b�t�@�E�f�v�X�o�b�t�@�̃N���A.
  m_commandList->ClearRenderTargetView(m_hColorRTV, clearColor, 0, nullptr);
  m_commandList->ClearDepthStencilView(m_hDepthDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // �e�N�X�`����`���ɃZ�b�g.
  D3D12_CPU_DESCRIPTOR_HANDLE colorDescriptors[] = { m_hColorRTV };
  D3D12_CPU_DESCRIPTOR_HANDLE depthDescirptor = m_hDepthDSV;
  m_commandList->OMSetRenderTargets(
    _countof(colorDescriptors), colorDescriptors, FALSE, &depthDescirptor);

  // �r���[�|�[�g�ƃV�U�[�̃Z�b�g.
  auto width = RenderTexWidth;
  auto height = RenderTexHeight;
  auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(width), float(height));
  auto scissorRect = CD3DX12_RECT(0, 0, LONG(width), LONG(height));
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissorRect);

  // �e�B�[�|�b�g�̕`��.
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

  // �`���A�e�N�X�`���Ƃ��Ďg�����߂̃o���A��ݒ�.
  auto barrierToSRV = CD3DX12_RESOURCE_BARRIER::Transition(
    m_colorRT.Get(),
    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  );
  m_commandList->ResourceBarrier(1, &barrierToSRV);
}

void RenderToTextureApp::RenderToMain()
{
  auto rtv = m_swapchain->GetCurrentRTV();
  auto dsv = m_defaultDepthDSV;

  // �J���[�o�b�t�@(�����_�[�^�[�Q�b�g�r���[)�̃N���A
  float m_clearColor[4] = { 0.5f,0.75f,1.0f,0 };
  m_commandList->ClearRenderTargetView(rtv, m_clearColor, 0, nullptr);

  // �f�v�X�o�b�t�@(�f�v�X�X�e���V���r���[)�̃N���A
  m_commandList->ClearDepthStencilView(
    dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // �`�����Z�b�g
  m_commandList->OMSetRenderTargets(1, &(D3D12_CPU_DESCRIPTOR_HANDLE)rtv,
    FALSE, &(D3D12_CPU_DESCRIPTOR_HANDLE)dsv);

  auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(m_width), float(m_height));
  auto scissorRect = CD3DX12_RECT(0, 0, LONG(m_width), LONG(m_height));

  // �r���[�|�[�g�ƃV�U�[�̃Z�b�g
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissorRect);

  auto mtxWorld = XMMatrixRotationY(m_frameCount*0.01f);
  auto mtxView = XMMatrixLookAtRH(
    XMVectorSet(0.0f, 0.0f, 3.0f, 0.0f),
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


  // �����_�[�^�[�Q�b�g����X���b�v�`�F�C���\���\��
  auto barrierToPresent = m_swapchain->GetBarrierToPresent();
  auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
    m_colorRT.Get(),
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET
  );
  CD3DX12_RESOURCE_BARRIER barriers[] = {
    barrierToPresent, barrierToRT
  };
  
  m_commandList->ResourceBarrier(_countof(barriers), barriers);
}

void RenderToTextureApp::PrepareTeapot()
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

  // �R�s�[�������I�������͊e�o�b�t�@�̃X�e�[�g��K�؂ɕύX���Ă���.
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

  WaitForIdleGPU(); // ��������������̂�҂�.

  ComPtr<ID3DBlob> errBlob, vs, ps;
  hr = CompileShaderFromFile(L"modelVS.hlsl", L"vs_6_0", vs, errBlob);
  ThrowIfFailed(hr, "VertexShader error");
  hr = CompileShaderFromFile(L"modelPS.hlsl", L"ps_6_0", ps, errBlob);
  ThrowIfFailed(hr, "PixelShader error");

  // ���[�g�V�O�l�`���\�z
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

  // �C���v�b�g���C�A�E�g
  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(TeapotModel::Vertex, Position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, offsetof(TeapotModel::Vertex,Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
  };

  // �p�C�v���C���X�e�[�g�I�u�W�F�N�g�̐���.
  auto psoDesc = book_util::CreateDefaultPsoDesc(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    vs, ps, book_util::CreateTeapotModelRasterizerDesc(),
    inputElementDesc, _countof(inputElementDesc), m_model.rootSig);
  hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_model.pipeline));
  ThrowIfFailed(hr, "CreateGraphicsPipelineState failed.");

  // �萔�o�b�t�@����
  bufferSize = sizeof(SceneParameter);
  bufferSize = book_util::RoundupConstantBufferSize(bufferSize);
  auto cbDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
  for (auto& cb : CreateConstantBuffers(cbDesc))
  {
    m_model.sceneCB.push_back(cb);
  }
}

void RenderToTextureApp::PreparePlane()
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

  // �R�s�[�������I�������͊e�o�b�t�@�̃X�e�[�g��K�؂ɕύX���Ă���.
  auto barrierVB = CD3DX12_RESOURCE_BARRIER::Transition(
    m_plane.resourceVB.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
  );
  m_commandList->ResourceBarrier(1, &barrierVB);
  m_commandList->Close();

  ID3D12CommandList* command[] = { m_commandList.Get() };
  m_commandQueue->ExecuteCommandLists(1, command);

  WaitForIdleGPU(); // ��������������̂�҂�.

  ComPtr<ID3DBlob> errBlob, vs, ps;
  hr = CompileShaderFromFile(L"planeVS.hlsl", L"vs_6_0", vs, errBlob);
  ThrowIfFailed(hr, "VertexShader error");
  hr = CompileShaderFromFile(L"planePS.hlsl", L"ps_6_0", ps, errBlob);
  ThrowIfFailed(hr, "PixelShader error");

  // ���[�g�V�O�l�`���\�z
  CD3DX12_DESCRIPTOR_RANGE rangeSrv;
  rangeSrv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 ���W�X�^.
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

  // �C���v�b�g���C�A�E�g
  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
  };

  // �p�C�v���C���X�e�[�g�I�u�W�F�N�g�̐���.
  auto rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // ���ʕ`�悵��������.

  auto psoDesc = book_util::CreateDefaultPsoDesc(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    vs, ps, rasterizerDesc,
    inputElementDesc, _countof(inputElementDesc), m_plane.rootSig);
  hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_plane.pipeline));
  ThrowIfFailed(hr, "CreateGraphicsPipelineState failed.");

  // �萔�o�b�t�@����
  bufferSize = sizeof(SceneParameter);
  bufferSize = book_util::RoundupConstantBufferSize(bufferSize);
  auto cbDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
  for (auto& cb : CreateConstantBuffers(cbDesc))
  {
    m_plane.sceneCB.push_back(cb);
  }
}
