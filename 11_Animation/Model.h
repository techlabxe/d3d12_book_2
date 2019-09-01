#pragma once
#include "D3D12AppBase.h"
#include <wrl.h>
#include <DirectXMath.h>

#include <unordered_map>

class Material
{
public:
  struct MaterialParameters {
    DirectX::XMFLOAT4 diffuse;
    DirectX::XMFLOAT4 ambient;
    DirectX::XMFLOAT4 specular;
    UINT useTexture;
    UINT edgeFlag;
  };
  Material(const MaterialParameters& params) : m_parameters(params) { }

  DirectX::XMFLOAT4 GetDiffuse() const  { return m_parameters.diffuse; }
  DirectX::XMFLOAT4 GetAmbient() const  { return m_parameters.ambient; }
  DirectX::XMFLOAT4 GetSpecular() const { return m_parameters.specular; }
  bool GetEdgeFlag() const { return m_parameters.edgeFlag != 0; }

  struct Resource
  {
    Microsoft::WRL::ComPtr<ID3D12Resource1> resource;
    DescriptorHandle descriptor;
  };
  void SetTexture(Resource& resource) { m_texture = resource; }
  void SetConstantBuffer(Resource& resource) { m_materialConstantBuffer = resource; }

  Resource GetConstantBuffer() const { return m_materialConstantBuffer; }
  DescriptorHandle GetTextureDescriptor() const { return m_texture.descriptor; }

  bool HasTexture() const;

  void Update(D3D12AppBase* app);
private:
  MaterialParameters m_parameters;
  Resource m_texture;
  Resource m_materialConstantBuffer;
};

class Bone
{
public:
  using XMFLOAT3 = DirectX::XMFLOAT3;
  using XMFLOAT4 = DirectX::XMFLOAT4;
  using XMFLOAT4X4 = DirectX::XMFLOAT4X4;
  using XMVECTOR = DirectX::XMVECTOR;
  using XMMATRIX = DirectX::XMMATRIX;

  Bone();
  Bone(const std::string& name);

  void SetTranslation(const XMFLOAT3& trans);
  void SetTranslation(const XMVECTOR trans) { m_translation = trans; }

  void SetRotation(const XMFLOAT4& rot);
  void SetRotation(const XMVECTOR rot) { m_rotation = rot; }

  XMVECTOR GetTranslation() const { return m_translation; }
  XMVECTOR GetRotation() const { return m_rotation; }

  const std::string& GetName() const { return m_name; }
  XMMATRIX GetLocalMatrix() const { return m_mtxLocal; }
  XMMATRIX GetWorldMatrix() const { return m_mtxWorld; }
  XMMATRIX GetInvBindMatrix() const { return m_mtxInvBind; }

  void UpdateLocalMatrix();
  void UpdateWorldMatrix();
  void UpdateMatrices();

  void SetInitialTranslation(const XMFLOAT3& trans);

  void SetParent(Bone* parent)
  {
    m_parent = parent;
    if (m_parent != nullptr)
    {
      m_parent->AddChild(this);
    }
  }
  Bone* GetParent() const
  {
    return m_parent;
  }
  XMVECTOR GetInitialTranslation() const { return m_initialTranslation; }

  void SetInvBindMatrix(const XMMATRIX& mtxInvBind) { m_mtxInvBind = mtxInvBind; }

private:
  void AddChild(Bone* bone) { m_children.push_back(bone); }
  std::string m_name;
  Bone* m_parent;
  std::vector<Bone*> m_children;

  XMVECTOR m_translation;
  XMVECTOR m_rotation;
  XMVECTOR m_initialTranslation;
  XMMATRIX m_mtxLocal;
  XMMATRIX m_mtxWorld;
  XMMATRIX m_mtxInvBind;
};

class PMDBoneIK
{
public:
  PMDBoneIK() : m_effector(nullptr), m_target(nullptr), m_angleLimit(0.0f), m_iteration(0) { }
  PMDBoneIK(Bone* target, Bone* eff) : m_effector(eff), m_target(target) { }

  Bone* GetEffector() const { return m_effector; }
  Bone* GetTarget() const { return m_target; }
  float GetAngleWeight() const { return m_angleLimit; }
  const std::vector<Bone*>& GetChains() const { return m_ikChains; }
  int GetIterationCount() const { return m_iteration; }

  void SetAngleLimit(float angle) { m_angleLimit = angle; }
  void SetIterationCount(int iterationCount) { m_iteration = iterationCount; }
  void SetIkChains(std::vector<Bone*>& chains) { m_ikChains = chains; }
private:
  Bone* m_effector;
  Bone* m_target;
  std::vector<Bone*> m_ikChains;
  float m_angleLimit;
  int   m_iteration;
};

class Model
{
  template<class T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;
  using Bundle = ComPtr<ID3D12GraphicsCommandList>;
  using BundleList = std::vector<Bundle>;
  using XMFLOAT2 = DirectX::XMFLOAT2;
  using XMFLOAT3 = DirectX::XMFLOAT3;
  using XMFLOAT4 = DirectX::XMFLOAT4;
  using XMFLOAT4X4 = DirectX::XMFLOAT4X4;
  using XMUINT2 = DirectX::XMUINT2;

  using PipelineState = ComPtr<ID3D12PipelineState>;
  using RootSignature = ComPtr<ID3D12RootSignature>;
  using Buffer = ComPtr<ID3D12Resource1>;
  using Texture = ComPtr<ID3D12Resource1>;
  using GraphicsCommandList = ComPtr<ID3D12GraphicsCommandList>;
public:
  void Prepare(D3D12AppBase* app, const char* filename);
  void Cleanup(D3D12AppBase* app);

  struct PMDVertex
  {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 uv;
    XMUINT2  boneIndices;
    XMFLOAT2 boneWeights;
    UINT edgeFlag;
  };
  struct SceneParameter
  {
    XMFLOAT4X4 view;
    XMFLOAT4X4 proj;
    XMFLOAT4 lightDirection;
    XMFLOAT4 eyePosition;
    XMFLOAT4 outlineColor;
    XMFLOAT4X4 lightViewProj;
    XMFLOAT4X4 lightViewProjBias;
  };
  struct BoneParameter
  {
    XMFLOAT4X4 bone[512];
  };

  void SetSceneParameter(const SceneParameter& params) { m_sceneParameter = params; }
  void SetShadowMap(DescriptorHandle handle) { m_shadowMap = handle; }

  void UpdateMatrices();
  void Update(uint32_t imageIndex, D3D12AppBase* app);

  void Draw(D3D12AppBase* app, GraphicsCommandList commandList);
  void DrawShadow(D3D12AppBase* app, GraphicsCommandList commandList);

  // ボーン情報
  uint32_t GetBoneCount() const { return uint32_t(m_bones.size()); }
  const Bone* GetBone(int idx) const { return m_bones[idx]; }
  Bone* GetBone(int idx) { return m_bones[idx]; }

  // 表情モーフ情報.
  uint32_t GetFaceMorphCount() const { return uint32_t(m_faceOffsetInfo.size()); }
  int GetFaceMorphIndex(const std::string& faceName) const;
  void SetFaceMorphWeight(int index, float weight);

  // IK情報
  uint32_t GetBoneIKCount() const { return uint32_t(m_boneIkList.size()); }
  const PMDBoneIK& GetBoneIK(int idx) const { return m_boneIkList[idx]; }
private:
  void PrepareRootSignature(D3D12AppBase* app);
  void PreparePipelineStates(D3D12AppBase* app);
  void PrepareConstantBuffers(D3D12AppBase* app);
  void PrepareBundles(D3D12AppBase* app);
  void PrepareDummyTexture(D3D12AppBase* app);
  void ComputeMorph();

  SceneParameter m_sceneParameter;
  BoneParameter m_boneMatrices;
  std::vector<PMDVertex> m_hostMemVertices;
  std::vector<Material> m_materials;

  struct Mesh
  {
    uint32_t indexOffset;
    uint32_t indexCount;
  };
  std::vector<Mesh> m_meshes;
  std::vector<Bone*> m_bones;
  
  RootSignature m_rootSignature;
  std::unordered_map<std::string, PipelineState> m_pipelineStates;
  Bundle m_bundleNormalDraw;
  Bundle m_bundleOutline;
  Bundle m_bundleShadow;
  
  std::vector<BundleList> m_commandsShadow;

  Buffer m_indexBuffer;
  UINT m_indexBufferSize;
  std::vector<Buffer> m_vertexBuffers;
  std::vector<Buffer> m_sceneParameterCB;
  std::vector<Buffer> m_boneParameterCB;
  Texture m_textureDummy;
  
  DescriptorHandle m_shadowMap;
  DescriptorHandle m_dummyTexDescriptor;


  // 表情モーフベース頂点情報.
  struct PMDFaceBaseInfo
  {
    std::vector<uint32_t> indices;
    std::vector<XMFLOAT3> verticesPos;
  } m_faceBaseInfo;

  // 表情モーフオフセット頂点情報.
  struct PMDFaceInfo
  {
    std::string name;
    std::vector<uint32_t> indices;
    std::vector<XMFLOAT3> verticesOffset;
  };
  std::vector<PMDFaceInfo> m_faceOffsetInfo;
  std::vector<float> m_faceMorphWeights;

  std::vector<PMDBoneIK> m_boneIkList;
};
