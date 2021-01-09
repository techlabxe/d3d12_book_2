#include "Model.h"
#include "loader/PMDloader.h"

#include "D3D12AppBase.h"
#include <DirectXMath.h>
#include <DirectXTex.h>

#include <fstream>

using namespace std;
using namespace DirectX;

#define DRAW_GROUP_NORMAL std::string("normalDraw")
#define DRAW_GROUP_OUTLINE std::string("outlineDraw")
#define DRAW_GROUP_SHADOW std::string("shadowDraw")

inline Model::PMDVertex convertTo(const loader::PMDVertex& v)
{
  return Model::PMDVertex{
    v.getPosition(), v.getNormal(), v.getUV(),
    XMUINT2(v.getBoneIndex(0), v.getBoneIndex(1)),
    XMFLOAT2(v.getBoneWeight(0), v.getBoneWeight(1)),
    v.getEdgeFlag()
  };
}

inline XMFLOAT4 toFloat4(const XMFLOAT3& xyz, float a)
{
  return XMFLOAT4{
    xyz.x, xyz.y, xyz.z, a
  };
}

inline XMFLOAT3 operator+(const XMFLOAT3& a, const XMFLOAT3& b)
{
  return XMFLOAT3{
    a.x + b.x, a.y + b.y, a.z + b.z
  };
}
inline XMFLOAT3 operator-(const XMFLOAT3& a, const XMFLOAT3& b)
{
  return XMFLOAT3{
    a.x - b.x, a.y - b.y, a.z - b.z
  };
}
inline XMFLOAT3 operator*(const XMFLOAT3& a, float k)
{
  return XMFLOAT3{
    a.x * k, a.y * k, a.z * k
  };
}
inline XMFLOAT3& operator+=(XMFLOAT3& a,const XMFLOAT3& b)
{
  a.x += b.x;
  a.y += b.y;
  a.z += b.z;
  return a;
}

bool Material::HasTexture() const
{
  return m_parameters.useTexture != 0;
}

void Material::Update(D3D12AppBase* app)
{
  app->WriteToUploadHeapMemory(
    m_materialConstantBuffer.resource.Get(),
    sizeof(m_parameters),
    &m_parameters);
}

Bone::Bone() :
  m_name(), m_translation(), m_rotation(), m_initialTranslation(),
  m_parent(nullptr), m_mtxLocal(), m_mtxWorld(), m_mtxInvBind()
{
  m_rotation = XMQuaternionIdentity();
}
Bone::Bone(const std::string& name) :
  m_name(name), m_translation(), m_rotation(), m_initialTranslation(),
  m_parent(nullptr), m_mtxLocal(), m_mtxWorld(), m_mtxInvBind()
{
  m_rotation = XMQuaternionIdentity();
}
void Bone::SetTranslation(const XMFLOAT3& trans)
{
  m_translation = XMLoadFloat3(&trans);
}
void Bone::SetRotation(const XMFLOAT4& rot)
{
  m_rotation = XMLoadFloat4(&rot);
}

void Bone::UpdateLocalMatrix()
{
  m_mtxLocal = XMMatrixRotationQuaternion(m_rotation) * XMMatrixTranslationFromVector(m_translation);
}

void Bone::UpdateWorldMatrix()
{
  UpdateLocalMatrix();
  auto mtxParent = XMMatrixIdentity();
  if (m_parent)
  {
    mtxParent = m_parent->GetWorldMatrix();
  }
  m_mtxWorld = m_mtxLocal * mtxParent;
}

void Bone::UpdateMatrices()
{
  UpdateWorldMatrix();
  for (auto c : m_children)
  {
    c->UpdateMatrices();
  }
}

void Bone::SetInitialTranslation(const XMFLOAT3& trans)
{
  m_initialTranslation = XMLoadFloat3(&trans);
}

void Model::Prepare(D3D12AppBase* app, const char* filename)
{
  ifstream infile(filename, std::ios::binary);
  loader::PMDFile loader(infile);
  auto device = app->GetDevice();

  auto vertexCount = loader.getVertexCount();
  auto indexCount = loader.getIndexCount();
  m_hostMemVertices.resize(vertexCount);
  for (uint32_t i = 0; i < vertexCount; ++i)
  {
    m_hostMemVertices[i] = convertTo(loader.getVertex(i));
  }
  std::vector<uint32_t> modelIndices(indexCount);
  for (uint32_t i = 0; i < indexCount; ++i)
  {
    auto& v = modelIndices[i];
    v = loader.getIndices()[i];
  }

  m_indexBufferSize = indexCount * sizeof(UINT);
  auto ibDesc = CD3DX12_RESOURCE_DESC::Buffer(m_indexBufferSize);
  auto stagingIB = app->CreateResource(
    ibDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr, D3D12_HEAP_TYPE_UPLOAD
  );
  m_indexBuffer = app->CreateResource(
    ibDesc, 
    D3D12_RESOURCE_STATE_COPY_DEST,
    nullptr, D3D12_HEAP_TYPE_DEFAULT
  );
  app->WriteToUploadHeapMemory(stagingIB.Get(), uint32_t(ibDesc.Width), modelIndices.data());

  auto command = app->CreateCommandList();
  // Staging => Default へ転送[インデックスバッファ]
  command->CopyResource(m_indexBuffer.Get(), stagingIB.Get());
  // リソースステートを再セット.
  auto barrierIB = CD3DX12_RESOURCE_BARRIER::Transition(
    m_indexBuffer.Get(),
    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER
  );
  command->ResourceBarrier(1, &barrierIB);
  app->FinishCommandList(command);

  // 頂点バッファ作成.
  m_vertexBuffers.resize(D3D12AppBase::FrameBufferCount);
  auto vbDesc = CD3DX12_RESOURCE_DESC::Buffer(
    vertexCount * sizeof(PMDVertex)
  );
  for (UINT i = 0; i < D3D12AppBase::FrameBufferCount; ++i)
  {
    m_vertexBuffers[i] = app->CreateResource(
      vbDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      D3D12_HEAP_TYPE_UPLOAD
    );
  }

  // マテリアル読み込み.
  const auto materialCount = loader.getMaterialCount();
  for (uint32_t i = 0; i < materialCount; ++i)
  {
    const auto& src = loader.getMaterial(i);
    Material::MaterialParameters materialParams{};
    materialParams.diffuse = toFloat4(src.getDiffuse(), src.getAlpha());
    materialParams.ambient = toFloat4(src.getAmbient(), 0.0f);
    materialParams.specular = toFloat4(src.getSpecular(), src.getShininess());
    materialParams.useTexture = 0;
    materialParams.edgeFlag = src.getEdgeFlag();

    auto textureFileName = src.getTexture();
    auto hasSphereMap = textureFileName.find('*');
    if (hasSphereMap != std::string::npos)
    {
      textureFileName = textureFileName.substr(0, hasSphereMap);
    }
    if (!textureFileName.empty())
    {
      materialParams.useTexture = 1;
    }
    Material material(materialParams);
    auto cbDesc = CD3DX12_RESOURCE_DESC::Buffer(uint32_t(sizeof(materialParams)));
    auto constantBuffers = app->CreateConstantBuffers(cbDesc, 1);
    Material::Resource materialCB;
    materialCB.resource = constantBuffers[0];
    material.SetConstantBuffer(materialCB);

    if (materialParams.useTexture)
    {
      // テクスチャの読み込み.
      ScratchImage image;
      HRESULT hr;
      hr = LoadFromWICFile(book_util::ConvertWstring(textureFileName).c_str(), 0, nullptr, image);
      ThrowIfFailed(hr, "LoadFromWICFile Failed.");
      auto metadata = image.GetMetadata();
      
      vector<D3D12_SUBRESOURCE_DATA> subresources;
      ComPtr<ID3D12Resource> texture;
      CreateTexture(device.Get(), metadata, &texture);

      // 転送元となるステージング用バッファを準備.
      PrepareUpload( device.Get(),
        image.GetImages(), image.GetImageCount(),
        metadata, subresources);
      const auto totalBytes = GetRequiredIntermediateSize(
        texture.Get(), 0, UINT(subresources.size()));
      auto stagingTex= app->CreateResource(
        CD3DX12_RESOURCE_DESC::Buffer(totalBytes),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        D3D12_HEAP_TYPE_UPLOAD
      );
      // Staging=>Texture転送.
      command = app->CreateCommandList();
      UpdateSubresources(
        command.Get(),
        texture.Get(), stagingTex.Get(), 
        0, 0, uint32_t(subresources.size()),
        subresources.data());

      // リソースバリアをセット
      auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, 
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      command->ResourceBarrier(1, &barrier);

      app->FinishCommandList(command);

      // テクスチャ参照のためのディスクリプタ準備.
      D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
      srvDesc.Format = metadata.format;
      srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      auto descriptor = app->GetDescriptorManager()->Alloc();
      device->CreateShaderResourceView(
        texture.Get(), &srvDesc, descriptor);

      Material::Resource res;
      texture.As(&res.resource);
      res.descriptor = descriptor;
      material.SetTexture(res);
    }

    material.Update(app);
    m_materials.emplace_back(material);
  }

  // 描画用メッシュ情報構築.
  for (uint32_t i = 0,offset=0; i < materialCount; ++i)
  {
    const auto& src = loader.getMaterial(i);
    uint32_t indexCount = src.getNumberOfPolygons();

    m_meshes.emplace_back(
      Mesh{ offset, indexCount });
    offset += indexCount;
  }
  
  // ボーン情報構築.
  uint32_t boneCount = loader.getBoneCount();
  m_bones.reserve(boneCount);
  for (uint32_t i = 0; i < boneCount; ++i)
  {
    const auto& boneSrc = loader.getBone(i);
    auto index = boneSrc.getParent();

    auto bone = new Bone(boneSrc.getName());
    auto translation = boneSrc.getPosition();
    if (index != 0xFFFFu)
    {
      const auto& parent = loader.getBone(boneSrc.getParent());
      translation = translation - parent.getPosition();
    }
    bone->SetTranslation(translation);
    bone->SetInitialTranslation(translation);

    // バインド逆行列をグローバル位置より求める.
    const auto bonePos = boneSrc.getPosition();
    auto m = XMMatrixTranslationFromVector(XMLoadFloat3(&bonePos));
    bone->SetInvBindMatrix(XMMatrixInverse(nullptr, m));

    m_bones.push_back(bone);
  }
  for (uint32_t i = 0; i < boneCount; ++i)
  {
    const auto& boneSrc = loader.getBone(i);
    auto bone = m_bones[i];
    uint32_t index = boneSrc.getParent();
    if (index != 0xFFFFu)
    {
      bone->SetParent(m_bones[index]);
    }
  }
  UpdateMatrices();

  // 表情モーフ情報読み込み.
  {
    // 表情ベース.
    auto baseFace = loader.getFaceBase();
    auto vertexCount = baseFace.getVertexCount();
    auto indexCount = baseFace.getIndexCount();
    m_faceBaseInfo.verticesPos.resize(vertexCount);
    m_faceBaseInfo.indices.resize(indexCount);
    auto sizeVB = vertexCount * sizeof(XMFLOAT3);
    auto sizeIB = indexCount * sizeof(uint32_t);
    memcpy(m_faceBaseInfo.verticesPos.data(), baseFace.getFaceVertices(), sizeVB);
    memcpy(m_faceBaseInfo.indices.data(), baseFace.getFaceIndices(), sizeIB);

    // オフセット表情モーフ.
    auto faceCount = loader.getFaceCount() - 1;
    m_faceOffsetInfo.resize(faceCount);
    for (uint32_t i = 0; i < faceCount; ++i) // 1から始めるのは 0 が base のため.
    {
      auto faceSrc = loader.getFace(i + 1);
      auto& face = m_faceOffsetInfo[i];
      face.name = faceSrc.getName();

      indexCount = faceSrc.getIndexCount();
      vertexCount = faceSrc.getVertexCount();
      face.indices.resize(indexCount);
      face.verticesOffset.resize(vertexCount);
      sizeVB = vertexCount * sizeof(XMFLOAT3);
      sizeIB = indexCount * sizeof(uint32_t);
      memcpy(face.verticesOffset.data(), faceSrc.getFaceVertices(), sizeVB);
      memcpy(face.indices.data(), faceSrc.getFaceIndices(), sizeIB);
    }

    m_faceMorphWeights.resize(faceCount);
  }

  // IKボーン情報を読み込む.
  auto ikBoneCount = loader.getIkCount();
  m_boneIkList.resize(ikBoneCount);
  for (uint32_t i = 0; i < ikBoneCount; ++i)
  {
    const auto& ik = loader.getIk(i);
    auto& boneIk = m_boneIkList[i];
    auto targetBone = m_bones[ik.getTargetBoneId()];
    auto effectorBone = m_bones[ik.getBoneEff()];

    boneIk = PMDBoneIK(targetBone, effectorBone);
    boneIk.SetAngleLimit(ik.getAngleLimit());
    boneIk.SetIterationCount(ik.getIterations());

    auto chains = ik.getChains();
    std::vector<Bone*> ikChains;
    ikChains.reserve(chains.size());
    for (auto& id : chains)
    {
      ikChains.push_back(m_bones[id]);
    }
    boneIk.SetIkChains(ikChains);
  }

  PrepareRootSignature(app);
  PreparePipelineStates(app);
  PrepareConstantBuffers(app);
  PrepareDummyTexture(app);
  PrepareBundles(app);
}

void Model::Cleanup(D3D12AppBase* app)
{
  for (auto& b : m_bones)
  {
    delete b;
  }
  m_bones.clear();
}

void Model::UpdateMatrices()
{
  for (auto& b : m_bones)
  {
    if (b->GetParent())
      continue;

    b->UpdateMatrices();
  }
}

void Model::Update(uint32_t imageIndex, D3D12AppBase* app)
{
  auto dstSceneCB = m_sceneParameterCB[imageIndex];
  app->WriteToUploadHeapMemory(
    dstSceneCB.Get(), sizeof(SceneParameter), &m_sceneParameter
  );

  // ボーン行列を定数バッファへ書き込む.
  for (uint32_t i = 0; i < uint32_t(m_bones.size()); ++i)
  {
    auto bone = m_bones[i];
    auto m = bone->GetInvBindMatrix() * bone->GetWorldMatrix();
    XMStoreFloat4x4(&m_boneMatrices.bone[i], XMMatrixTranspose(m));
  }
  auto dstBoneCB = m_boneParameterCB[imageIndex];
  app->WriteToUploadHeapMemory(
    dstBoneCB.Get(), sizeof(BoneParameter), &m_boneMatrices
  );

  // モーフ計算と頂点バッファの更新.
  ComputeMorph();
  auto dstVB = m_vertexBuffers[imageIndex];
  uint32_t sizeVB = uint32_t(sizeof(PMDVertex) * m_hostMemVertices.size());
  app->WriteToUploadHeapMemory(dstVB.Get(), sizeVB, m_hostMemVertices.data());
}

void Model::Draw(D3D12AppBase* app, ComPtr<ID3D12GraphicsCommandList> commandList)
{
  uint32_t index = app->GetSwapchain()->GetCurrentBackBufferIndex();
  auto sceneCB = m_sceneParameterCB[index];
  auto boneCB = m_boneParameterCB[index];

  commandList->SetGraphicsRootSignature(m_rootSignature.Get());
  commandList->SetGraphicsRootConstantBufferView(0, sceneCB->GetGPUVirtualAddress());
  commandList->SetGraphicsRootConstantBufferView(1, boneCB->GetGPUVirtualAddress());
  commandList->SetGraphicsRootDescriptorTable(4, m_shadowMap);


  // 現在の頂点バッファをセットする.
  D3D12_VERTEX_BUFFER_VIEW vbView{};
  vbView.BufferLocation = m_vertexBuffers[index]->GetGPUVirtualAddress();
  vbView.StrideInBytes = UINT(sizeof(PMDVertex));
  vbView.SizeInBytes = UINT(vbView.StrideInBytes * m_hostMemVertices.size());
  commandList->IASetVertexBuffers(0, 1, &vbView);

  // 通常描画を行う.
  commandList->ExecuteBundle(m_bundleNormalDraw.Get());

  // 輪郭線描画を行う.
  commandList->ExecuteBundle(m_bundleOutline.Get());
}

void Model::DrawShadow(D3D12AppBase* app, ComPtr<ID3D12GraphicsCommandList> commandList)
{
  uint32_t index = app->GetSwapchain()->GetCurrentBackBufferIndex();
  auto sceneCB = m_sceneParameterCB[index];
  auto boneCB = m_boneParameterCB[index];

  commandList->SetGraphicsRootSignature(m_rootSignature.Get());
  commandList->SetGraphicsRootConstantBufferView(0, sceneCB->GetGPUVirtualAddress());
  commandList->SetGraphicsRootConstantBufferView(1, boneCB->GetGPUVirtualAddress());

  // 現在の頂点バッファをセットする.
  D3D12_VERTEX_BUFFER_VIEW vbView{};
  vbView.BufferLocation = m_vertexBuffers[index]->GetGPUVirtualAddress();
  vbView.StrideInBytes = UINT(sizeof(PMDVertex));
  vbView.SizeInBytes = UINT(vbView.StrideInBytes * m_hostMemVertices.size());
  commandList->IASetVertexBuffers(0, 1, &vbView);

  // シャドウマップのための描画を行う.
  commandList->ExecuteBundle(m_bundleShadow.Get());
}

int Model::GetFaceMorphIndex(const std::string& faceName) const
{
  int ret = -1;
  for (uint32_t i = 0; i < m_faceOffsetInfo.size(); ++i)
  {
    const auto& face = m_faceOffsetInfo[i];
    if (face.name == faceName)
    {
      ret = i;
      break;
    }
  }
  return ret;
}

void Model::SetFaceMorphWeight(int index, float weight)
{
  if (index < 0)
  {
    return;
  }
  m_faceMorphWeights[index] = weight;
}

void Model::PrepareRootSignature(D3D12AppBase* app)
{
  CD3DX12_DESCRIPTOR_RANGE diffuseTexRange;
  diffuseTexRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 割り当て.

  CD3DX12_DESCRIPTOR_RANGE shadowTexRange;
  shadowTexRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1 割り当て.

  array<CD3DX12_ROOT_PARAMETER, 5> rootParams;
  rootParams[0].InitAsConstantBufferView(0); // sceneParameter
  rootParams[1].InitAsConstantBufferView(1); // boneParameter
  rootParams[2].InitAsConstantBufferView(2); // materialParameter
  rootParams[3].InitAsDescriptorTable(1, &diffuseTexRange, D3D12_SHADER_VISIBILITY_PIXEL);
  rootParams[4].InitAsDescriptorTable(1, &shadowTexRange, D3D12_SHADER_VISIBILITY_PIXEL);

  array<CD3DX12_STATIC_SAMPLER_DESC,2> samplerDesc;
  samplerDesc[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  samplerDesc[1].Init(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER);
  samplerDesc[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;

  CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
  rootSignatureDesc.Init(
    UINT(rootParams.size()), rootParams.data(),
    UINT(samplerDesc.size()), samplerDesc.data(),
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  HRESULT hr;
  ComPtr<ID3DBlob> signature, errBlob;
  hr = D3D12SerializeRootSignature(&rootSignatureDesc,
    D3D_ROOT_SIGNATURE_VERSION_1_0,
    &signature, &errBlob );
  ThrowIfFailed(hr, "D3D12SerializeRootSignature Failed.");

  auto device = app->GetDevice();
  hr = device->CreateRootSignature(
    0, signature->GetBufferPointer(), signature->GetBufferSize(),
    IID_PPV_ARGS(&m_rootSignature)
  );
  ThrowIfFailed(hr, "CreateRootSignature Failed.");
}

static void CheckCompileError(HRESULT hr, Microsoft::WRL::ComPtr<ID3DBlob>& errBlob)
{
  if (FAILED(hr) && errBlob )
  {
    const char* msg = reinterpret_cast<const char*>(errBlob->GetBufferPointer());
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
  }
  ThrowIfFailed(hr, "ShaderCompileError");
}

void Model::PreparePipelineStates(D3D12AppBase* app)
{
  HRESULT hr;
  ComPtr<ID3DBlob> errBlob;

  auto rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  rasterizerDesc.FrontCounterClockwise = true;
  rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

  using Shader = ComPtr<ID3DBlob>;
  Shader modelVS, modelPS;
  CheckCompileError(
    CompileShaderFromFile(L"modelVS.hlsl", L"vs_6_0", modelVS, errBlob), errBlob);
  CheckCompileError(
    CompileShaderFromFile(L"modelPS.hlsl", L"ps_6_0", modelPS, errBlob), errBlob);

  Shader modelOutlineVS, modelOutlinePS;
  CheckCompileError(
    CompileShaderFromFile(L"outlineVS.hlsl", L"vs_6_0", modelOutlineVS, errBlob), errBlob);
  CheckCompileError(
    CompileShaderFromFile(L"outlinePS.hlsl", L"ps_6_0", modelOutlinePS, errBlob), errBlob);

  Shader shadowVS, shadowPS;
  CheckCompileError(
    CompileShaderFromFile(L"shadowVS.hlsl", L"vs_6_0", shadowVS, errBlob), errBlob);
  CheckCompileError(
    CompileShaderFromFile(L"shadowPS.hlsl", L"ps_6_0", shadowPS, errBlob), errBlob);

  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "BLENDINDICES", 0, DXGI_FORMAT_R32G32_UINT,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "BLENDWEIGHTS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "EDGEFLAG", 0, DXGI_FORMAT_R32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  auto modelPsoDesc = book_util::CreateDefaultPsoDesc(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    modelVS, modelPS, rasterizerDesc,
    inputElementDesc, _countof(inputElementDesc),
    m_rootSignature.Get()
  );
  modelPsoDesc.BlendState.RenderTarget[0].BlendEnable = true;

  auto outlineRS = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  outlineRS.FrontCounterClockwise = true;
  outlineRS.CullMode = D3D12_CULL_MODE_FRONT;
  auto outlinePsoDesc = book_util::CreateDefaultPsoDesc(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    modelOutlineVS, modelOutlinePS,
    outlineRS,
    inputElementDesc, _countof(inputElementDesc),
    m_rootSignature.Get()
  );
  auto shadowPsoDesc = book_util::CreateDefaultPsoDesc(
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    shadowVS, shadowPS, 
    rasterizerDesc,
    inputElementDesc, _countof(inputElementDesc),
    m_rootSignature.Get()
  );
  shadowPsoDesc.BlendState.RenderTarget[0].BlendEnable = true;

  auto device = app->GetDevice();
  ComPtr<ID3D12PipelineState> pso;
  hr = device->CreateGraphicsPipelineState(
    &modelPsoDesc, IID_PPV_ARGS(&pso)
  );
  ThrowIfFailed(hr, "CreateGraphicsPipelineState Failed(normalDraw).");
  m_pipelineStates[DRAW_GROUP_NORMAL] = pso;
  hr = device->CreateGraphicsPipelineState(
    &outlinePsoDesc, IID_PPV_ARGS(&pso));
  ThrowIfFailed(hr, "CreateGraphicsPipelineState Failed(outlineDraw).");
  m_pipelineStates[DRAW_GROUP_OUTLINE] = pso;
  hr = device->CreateGraphicsPipelineState(
    &shadowPsoDesc, IID_PPV_ARGS(&pso));
  ThrowIfFailed(hr, "CreateGraphicsPipelineState Failed(shadowDraw).");
  m_pipelineStates[DRAW_GROUP_SHADOW] = pso;
}

void Model::PrepareConstantBuffers(D3D12AppBase* app)
{
  auto sceneParamDesc = CD3DX12_RESOURCE_DESC::Buffer(
    sizeof(SceneParameter)
  );
  m_sceneParameterCB = app->CreateConstantBuffers(sceneParamDesc);

  auto boneParamDesc = CD3DX12_RESOURCE_DESC::Buffer(
    sizeof(BoneParameter)
  );
  m_boneParameterCB = app->CreateConstantBuffers(boneParamDesc);
}

void Model::PrepareBundles(D3D12AppBase* app)
{
  auto imageCount = D3D12AppBase::FrameBufferCount;
  m_bundleNormalDraw = app->CreateBundleCommandList();
  ID3D12DescriptorHeap* heaps[] = {
    app->GetDescriptorManager()->GetHeap().Get(),
  };
  m_bundleNormalDraw->SetDescriptorHeaps(1, heaps);
  m_bundleNormalDraw->SetGraphicsRootSignature(m_rootSignature.Get());
  m_bundleNormalDraw->SetPipelineState(m_pipelineStates[DRAW_GROUP_NORMAL].Get());
  m_bundleNormalDraw->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  D3D12_INDEX_BUFFER_VIEW ibView{};
  ibView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
  ibView.Format = DXGI_FORMAT_R32_UINT;
  ibView.SizeInBytes = m_indexBufferSize;
  m_bundleNormalDraw->IASetIndexBuffer(&ibView);

  for (uint32_t i = 0; i < uint32_t(m_materials.size()); ++i)
  {
    auto mesh = m_meshes[i];
    const auto& material = m_materials[i];

    auto materialCB = material.GetConstantBuffer().resource;
    m_bundleNormalDraw->SetGraphicsRootConstantBufferView(2, materialCB->GetGPUVirtualAddress());

    auto textureDescriptor = m_dummyTexDescriptor;
    if (material.HasTexture())
    {
      textureDescriptor = material.GetTextureDescriptor();
    }
    m_bundleNormalDraw->SetGraphicsRootDescriptorTable(3, textureDescriptor);
    m_bundleNormalDraw->DrawIndexedInstanced(mesh.indexCount, 1, mesh.indexOffset, 0, 0);
  }
  m_bundleNormalDraw->Close();

  // 輪郭線描画用Bundle
  m_bundleOutline = app->CreateBundleCommandList();
  m_bundleOutline->SetDescriptorHeaps(1, heaps);
  m_bundleOutline->SetGraphicsRootSignature(m_rootSignature.Get());
  m_bundleOutline->SetPipelineState(m_pipelineStates[DRAW_GROUP_OUTLINE].Get());
  m_bundleOutline->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_bundleOutline->IASetIndexBuffer(&ibView);
  for (uint32_t i = 0; i < uint32_t(m_materials.size()); ++i)
  {
    auto mesh = m_meshes[i];
    const auto& material = m_materials[i];
    auto materialCB = material.GetConstantBuffer().resource;
    if (material.GetEdgeFlag() == 0)
      continue;

    m_bundleOutline->SetGraphicsRootConstantBufferView(2, materialCB->GetGPUVirtualAddress());
    m_bundleOutline->DrawIndexedInstanced(mesh.indexCount, 1, mesh.indexOffset, 0, 0);
  }
  m_bundleOutline->Close();

  // シャドウ描画用Bundle
  m_bundleShadow = app->CreateBundleCommandList();
  m_bundleShadow->SetDescriptorHeaps(1, heaps);
  m_bundleShadow->SetGraphicsRootSignature(m_rootSignature.Get());
  m_bundleShadow->SetPipelineState(m_pipelineStates[DRAW_GROUP_SHADOW].Get());
  m_bundleShadow->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_bundleShadow->IASetIndexBuffer(&ibView);
  for (uint32_t i = 0; i < uint32_t(m_materials.size()); ++i)
  {
    auto mesh = m_meshes[i];
    const auto& material = m_materials[i];
    auto materialCB = material.GetConstantBuffer().resource;
    m_bundleShadow->SetGraphicsRootConstantBufferView(2, materialCB->GetGPUVirtualAddress());
    m_bundleShadow->DrawIndexedInstanced(mesh.indexCount, 1, mesh.indexOffset, 0, 0);
  }
  m_bundleShadow->Close();
}

void Model::PrepareDummyTexture(D3D12AppBase* app)
{
  auto device = app->GetDevice();
  ScratchImage image;
  image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
  auto metadata = image.GetMetadata();

  vector<D3D12_SUBRESOURCE_DATA> subresources;
  ComPtr<ID3D12Resource> texture;
  CreateTexture(device.Get(), metadata, &texture);

  // 転送元ステージングバッファの用意.
  PrepareUpload(device.Get(),
    image.GetImages(), image.GetImageCount(), metadata, subresources);
  auto totalBytes = GetRequiredIntermediateSize(
    texture.Get(), 0, UINT(subresources.size()));
  auto staging = app->CreateResource(
    CD3DX12_RESOURCE_DESC::Buffer(totalBytes),
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
    D3D12_HEAP_TYPE_UPLOAD );

  // Staging => Texture 転送.
  auto command = app->CreateCommandList();
  UpdateSubresources(command.Get(),
    texture.Get(), staging.Get(),
    0, 0, UINT(subresources.size()), subresources.data());
  app->FinishCommandList(command);

  // テクスチャのディスクリプタを準備.
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = metadata.format;
  srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  m_dummyTexDescriptor = app->GetDescriptorManager()->Alloc();
  device->CreateShaderResourceView(
    texture.Get(), &srvDesc, m_dummyTexDescriptor);
}

void Model::ComputeMorph()
{
  auto vertexCount = m_faceBaseInfo.verticesPos.size();
  // 位置のリセット.
  for (uint32_t i = 0; i < vertexCount; ++i)
  {
    auto offsetIndex = m_faceBaseInfo.indices[i];
    m_hostMemVertices[offsetIndex].position = m_faceBaseInfo.verticesPos[i];
  }

  // ウェイトに応じて頂点を変更.
  for (uint32_t faceIndex = 0; faceIndex < m_faceOffsetInfo.size(); ++faceIndex)
  {
    const auto& face = m_faceOffsetInfo[faceIndex];
    float w = m_faceMorphWeights[faceIndex];

    for (uint32_t i = 0; i < face.indices.size(); ++i)
    {
      auto baseVertexIndex = face.indices[i];
      auto displacement = face.verticesOffset[i];

      auto offsetIndex = m_faceBaseInfo.indices[baseVertexIndex];
      XMFLOAT3 offset = displacement * w;
      m_hostMemVertices[offsetIndex].position += offset;
    }
  }
}
