#include "BundleApp.h"
#include "TeapotModel.h"
#include <random>

using namespace DirectX;

static DirectX::XMFLOAT4 colorSet[] = {
  XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
  XMFLOAT4(1.0f, 0.65f, 1.0f, 1.0f),
  XMFLOAT4(0.1f, 0.5f, 1.0f, 1.0f),
  XMFLOAT4(0.6f, 1.0f, 0.8f, 1.0f),
};

BundleApp::BundleApp()
  :m_instancingCount(InstanceDataMax)
{
}

void BundleApp::Prepare()
{
  SetTitle("Bundle Sample");
  // �o�b�t�@�̓]�����s�����߂ɃR�}���h���X�g���g���̂ŏ�������.
  m_commandAllocators[m_frameIndex]->Reset();
  m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr);

  void* mapped;
  HRESULT hr;
  CD3DX12_RANGE range(0, 0);
  UINT bufferSize = sizeof(TeapotModel::TeapotVerticesPN);

  m_model.resourceVB = CreateBufferResource(
    D3D12_HEAP_TYPE_DEFAULT, bufferSize, D3D12_RESOURCE_STATE_COPY_DEST
  );

  auto uploadVB = CreateBufferResource(
    D3D12_HEAP_TYPE_UPLOAD, bufferSize, D3D12_RESOURCE_STATE_GENERIC_READ
  );

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
  m_model.resourceIB = CreateBufferResource(
    D3D12_HEAP_TYPE_DEFAULT, bufferSize, D3D12_RESOURCE_STATE_COPY_DEST
  );
  auto uploadIB = CreateBufferResource(
    D3D12_HEAP_TYPE_UPLOAD, bufferSize, D3D12_RESOURCE_STATE_GENERIC_READ
  );

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

  ComPtr<ID3DBlob> errBlob;
  hr = CompileShaderFromFile(
    L"VertexShader.hlsl", L"vs_6_0", m_vs, errBlob);
  ThrowIfFailed(hr, "VertexShader error");

  hr = CompileShaderFromFile(
    L"PixelShader.hlsl", L"ps_6_0", m_ps, errBlob);
  ThrowIfFailed(hr, "PixelShader error");

  // ���[�g�V�O�l�`���\�z
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
    IID_PPV_ARGS(&m_rootSignature)
  );
  ThrowIfFailed(hr, "CreateRootSignature failed.");

  // �C���v�b�g���C�A�E�g
  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(TeapotModel::Vertex, Position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, offsetof(TeapotModel::Vertex,Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
  };

  // �p�C�v���C���X�e�[�g�I�u�W�F�N�g�̐���.
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
  // �V�F�[�_�[�̃Z�b�g
  psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vs.Get());
  psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_ps.Get());
  // �u�����h�X�e�[�g�ݒ�
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  // ���X�^���C�U�[�X�e�[�g
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.RasterizerState.FrontCounterClockwise = true;

  // �o�͐��1�^�[�Q�b�g
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  // �f�v�X�o�b�t�@�̃t�H�[�}�b�g��ݒ�
  psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };

  // ���[�g�V�O�l�`���̃Z�b�g
  psoDesc.pRootSignature = m_rootSignature.Get();
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  // �}���`�T���v���ݒ�
  psoDesc.SampleDesc = { 1,0 };
  psoDesc.SampleMask = UINT_MAX; // �����Y���ƊG���o�Ȃ����x�����o�Ȃ��̂Œ���.

  hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipeline));
  ThrowIfFailed(hr, "CreateGraphicsPipelineState failed.");

  // �萔�o�b�t�@�̏���
  bufferSize = book_util::RoundupConstantBufferSize(sizeof(SceneParameter));
  for (UINT i = 0; i < FrameBufferCount; ++i) {
    auto cb = CreateBufferResource(
      D3D12_HEAP_TYPE_UPLOAD,
      bufferSize,
      D3D12_RESOURCE_STATE_GENERIC_READ
    );
    m_constantBuffers.push_back(cb);
  }

  std::vector<InstanceData> instanceData(InstanceDataMax);
  {
    std::random_device rnd;
    // �C���X�^���V���O���g����������ł���.
    for (UINT i = 0; i < InstanceDataMax; ++i)
    {
      float k = float((rnd()) % 360);
      float x = (i % 10) * 3.0f - 10.0f;
      float z = (i / 10) * -3.0f+  5.0f;

      auto t0 = XMMatrixTranslation(x, 0.0f, z);
      auto m0 = XMMatrixRotationZ(k);
      auto m1 = XMMatrixRotationX(k);

      auto& data = instanceData[i];
      XMStoreFloat4x4(&data.world, XMMatrixTranspose(m0 * m1 * t0));
      data.Color = colorSet[i % _countof(colorSet)];
    }
  }

  // �C���X�^���V���O�p�̃o�b�t�@������.
  bufferSize = sizeof(InstanceData) * InstanceDataMax;
  bufferSize = book_util::RoundupConstantBufferSize(bufferSize);
  for (UINT i = 0; i < FrameBufferCount; ++i)
  {
    auto cb = CreateBufferResource(
      D3D12_HEAP_TYPE_UPLOAD,
      bufferSize,
      D3D12_RESOURCE_STATE_GENERIC_READ
    );
    m_instanceBuffers.push_back(cb);

    cb->Map(0, nullptr, &mapped);
    memcpy(mapped, instanceData.data(), bufferSize);
    cb->Unmap(0, nullptr);
  }

  hr = m_device->CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&m_bundleAllocator)
  );
  ThrowIfFailed(hr, "CreateCommandAllocator(Bundle) failed.");

  for (UINT i = 0; i < FrameBufferCount; ++i)
  {
    ComPtr<ID3D12GraphicsCommandList> bundle;
    hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, m_bundleAllocator.Get(), nullptr, IID_PPV_ARGS(&bundle));
    ThrowIfFailed(hr, "CreateCommandList(Bundle) failed.");

    bundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    bundle->IASetVertexBuffers(0, 1, &m_model.vbView);
    bundle->IASetIndexBuffer(&m_model.ibView);
    bundle->SetGraphicsRootSignature(m_rootSignature.Get());
    bundle->SetPipelineState(m_pipeline.Get());
    bundle->SetGraphicsRootConstantBufferView(
      0, m_constantBuffers[i]->GetGPUVirtualAddress()
    );
    bundle->SetGraphicsRootConstantBufferView(
      1, m_instanceBuffers[i]->GetGPUVirtualAddress()
    );
    bundle->DrawIndexedInstanced(
      m_model.indexCount, m_instancingCount, 0, 0, 0);
    bundle->Close();

    m_bundles.emplace_back(bundle);
  }
}
void BundleApp::Cleanup()
{
}


void BundleApp::Render()
{
  SceneParameter sceneParam;
  XMStoreFloat4x4(&sceneParam.view,
    XMMatrixTranspose(
      XMMatrixLookAtRH(
        XMVectorSet(0.0f, 5.0f, 10.0f, 0.0f),
        XMVectorSet(0.0f, 2.0f, 0.0f, 0.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)))
  );
  XMStoreFloat4x4(&sceneParam.proj,
    XMMatrixTranspose(
      XMMatrixPerspectiveFovRH(XMConvertToRadians(45.0f), m_viewport.Width / m_viewport.Height, 0.1f, 1000.0f)
    )
  );


  m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

  m_commandAllocators[m_frameIndex]->Reset();
  m_commandList->Reset(
    m_commandAllocators[m_frameIndex].Get(),
    nullptr
  );

  // �X���b�v�`�F�C���\���\���烌���_�[�^�[�Q�b�g�`��\��
  auto barrierToRT = m_swapchain->GetBarrierToRenderTarget();
  m_commandList->ResourceBarrier(1, &barrierToRT);

  auto rtv = m_swapchain->GetCurrentRTV();
  auto dsv = m_defaultDepthDSV;

  // �J���[�o�b�t�@(�����_�[�^�[�Q�b�g�r���[)�̃N���A
  float clearColor[4] = { 0.5f,0.75f,1.0f,0 };
  m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

  // �f�v�X�o�b�t�@(�f�v�X�X�e���V���r���[)�̃N���A
  m_commandList->ClearDepthStencilView(
    dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // �`�����Z�b�g
  D3D12_CPU_DESCRIPTOR_HANDLE handleRtvs[] = { rtv };
  D3D12_CPU_DESCRIPTOR_HANDLE handleDsv = dsv;
  m_commandList->OMSetRenderTargets(1, handleRtvs, FALSE, &handleDsv);

  auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(m_width), float(m_height));
  auto scissorRect = CD3DX12_RECT(0, 0, LONG(m_width), LONG(m_height));

  // �r���[�|�[�g�ƃV�U�[�̃Z�b�g
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissorRect);

  ID3D12DescriptorHeap* heaps[] = { m_heap->GetHeap().Get() };
  m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

  auto cb = m_constantBuffers[m_frameIndex];
  {
    void* mapped;
    cb->Map(0, nullptr, &mapped);
    memcpy(mapped, &sceneParam, sizeof(sceneParam));
    cb->Unmap(0, nullptr);
  }

  auto teapot = m_bundles[m_frameIndex];
  m_commandList->ExecuteBundle(teapot.Get());

  // �����_�[�^�[�Q�b�g����X���b�v�`�F�C���\���\��
  auto barrierToPresent = m_swapchain->GetBarrierToPresent();
  m_commandList->ResourceBarrier(1, &barrierToPresent);

  m_commandList->Close();

  ID3D12CommandList* lists[] = { m_commandList.Get() };

  m_commandQueue->ExecuteCommandLists(1, lists);

  m_swapchain->Present(1, 0);
  m_swapchain->WaitPreviousFrame(m_commandQueue, m_frameIndex, GpuWaitTimeout);
}

BundleApp::Buffer BundleApp::CreateBufferResource(D3D12_HEAP_TYPE type, UINT bufferSize, D3D12_RESOURCE_STATES state)
{
  Buffer ret;
  HRESULT hr;

  const auto heapProps = CD3DX12_HEAP_PROPERTIES(type);
  const auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
  hr = m_device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &resDesc,
    state,
    nullptr,
    IID_PPV_ARGS(&ret)
  );
  ThrowIfFailed(hr, "CreateCommittedResource failed.");
  return ret;
}