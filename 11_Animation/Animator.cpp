#include "Animator.h"
#include <fstream>

#include "loader/PMDloader.h"

#include "Model.h"

using namespace std;
using namespace DirectX;

static float dFx(float ax, float ay, float t)
{
  float s = 1.0f - t;
  float v = -6.0f * s * t * t * ax + 3.0f * s * s * ax - 3.0f * t * t * ay + 6.0f * s * t * ay + 3.0f * t * t;
  return v;
}
static float fx(float ax, float ay, float t, float x0)
{
  float s = 1.0f - t;
  return 3.0f * s * s * t * ax + 3.0f * s * t * t * ay + t * t * t - x0;
}

static float funcBezierX(XMFLOAT4 k, float t)
{
  float s = 1.0f - t;
  return 3.0f * s * s * t * k.x + 3.0f * s * t * t * k.y + t * t * t;
}
static float funcBezierY(XMFLOAT4 k, float t)
{
  float s = 1.0f - t;
  return 3.0f * s * s * t * k.y + 3.0f * s * t * t * k.w + t * t * t;
}

/// クォータニオンからオイラー角としてのXYZの角度を求める関数.
static DirectX::XMFLOAT3 GetQuaternionToEulerXYZ(const DirectX::XMVECTOR& quat) {
  using namespace DirectX;
  XMFLOAT4 tmp; XMStoreFloat4(&tmp, quat);
  XMFLOAT3 ret;

  // XYZ回転角を求める.
  float x2 = tmp.x + tmp.x;
  float y2 = tmp.y + tmp.y;
  float z2 = tmp.z + tmp.z;
  float xz2 = tmp.x * z2;
  float wy2 = tmp.w * y2;
  float temp = -(xz2 - wy2);
  if (temp >= 1.0f) { temp = 1.0f; }
  else if (temp <= -1.0f) { temp = -1.0f; }
  float yRadian = asinf(temp);

  float xx2 = tmp.x * x2;
  float xy2 = tmp.x * y2;
  float zz2 = tmp.z * z2;
  float wz2 = tmp.w * z2;
  if (yRadian < XM_PI * 0.5f) {
    if (yRadian > -XM_PI * 0.5f) {
      float yz2 = tmp.y * z2;
      float wx2 = tmp.w * x2;
      float yy2 = tmp.y * y2;
      ret.x = atan2f((yz2 + wx2), (1.0f - (xx2 + yy2)));
      ret.y = yRadian;
      ret.z = atan2f((xy2 + wz2), (1.0f - (yy2 + zz2)));
    }
    else {
      ret.x = -atan2f((xy2 - wz2), (1.0f - (xx2 + zz2)));
      ret.y = yRadian;
      ret.z = 0.0f;
    }
  }
  else {
    ret.x = atan2f((xy2 - wz2), (1.0f - (xx2 + zz2)));
    ret.y = yRadian;
    ret.z = 0.0f;
  }
  return ret;
}
// オイラー角からクォータニオンを生成
static DirectX::XMVECTOR MakeQuaternionEuler(const DirectX::XMFLOAT3 & eulerXYZ) {
  float xRadian = eulerXYZ.x * 0.5f;
  float yRadian = eulerXYZ.y * 0.5f;
  float zRadian = eulerXYZ.z * 0.5f;
  float sinX = sinf(xRadian);
  float cosX = cosf(xRadian);
  float sinY = sinf(yRadian);
  float cosY = cosf(yRadian);
  float sinZ = sinf(zRadian);
  float cosZ = cosf(zRadian);

  XMFLOAT4 newQuaternion;
  newQuaternion.x = sinX * cosY * cosZ - cosX * sinY * sinZ;
  newQuaternion.y = cosX * sinY * cosZ + sinX * cosY * sinZ;
  newQuaternion.z = cosX * cosY * sinZ - sinX * sinY * cosZ;
  newQuaternion.w = cosX * cosY * cosZ + sinX * sinY * sinZ;

  return DirectX::XMLoadFloat4(&newQuaternion);
}


void Animator::Prepare(const char* filename)
{
  std::ifstream infile(filename, std::ios::binary);
  loader::VMDFile loader(infile);

  m_framePeriod = loader.getKeyframeCount();
  uint32_t nodeCount = loader.getNodeCount();
  for (uint32_t i = 0; i < nodeCount; ++i)
  {
    const auto& name = loader.getNodeName(i);
    m_nodeMap[name] = NodeAnimation();
    auto& keyframes = m_nodeMap[name];
    auto framesSrc = loader.getKeyframes(name);

    auto frameCount = uint32_t(framesSrc.size());
    std::vector<NodeAnimeFrame> frames(frameCount);
    for (uint32_t i = 0; i < frameCount; ++i)
    {
      auto& dst = frames[i];
      auto& src = framesSrc[i];
      dst.frame = src.getKeyframeNumber();
      dst.translation = src.getLocation();
      dst.rotation = src.getRotation();
      dst.interpX = src.getBezierParam(0);
      dst.interpY = src.getBezierParam(1);
      dst.interpZ = src.getBezierParam(2);
      dst.interpR = src.getBezierParam(3);
    }
    keyframes.SetKeyframes(frames);
  }

  uint32_t morphCount = loader.getMorphCount();
  for (uint32_t i = 0; i < morphCount; ++i)
  {
    const auto& name = loader.getMorphName(i);
    m_morphMap[name] = MorphAnimation();
    auto& keyframes = m_morphMap[name];
    auto framesSrc = loader.getMorphKeyframes(name);

    auto frameCount = uint32_t(framesSrc.size());
    std::vector<MorphAnimeFrame> frames(frameCount);
    for (uint32_t i = 0; i < frameCount; ++i)
    {
      auto& dst = frames[i];
      const auto& src = framesSrc[i];
      dst.frame = src.getKeyframeNumber();
      dst.weight = src.getWeight();
    }
    keyframes.SetKeyframes(frames);
  }
}

void Animator::Cleanup()
{
}

void Animator::UpdateAnimation(uint32_t animeFrame)
{
  if (m_model == nullptr)
  {
    return;
  }

  // 全てのノードで指定されたフレームでの値を計算する.
  UpdateNodeAnimation(animeFrame);
  UpdateMorthAnimation(animeFrame);

  UpdateIKchains();
}

void Animator::UpdateNodeAnimation(uint32_t animeFrame)
{
  auto boneCount = m_model->GetBoneCount();
  for (uint32_t i = 0; i < boneCount; ++i) {
    auto bone = m_model->GetBone(i);

    auto itr = m_nodeMap.find(bone->GetName());
    if (itr == m_nodeMap.end())
    {
      continue;
    }
    auto segment = (*itr).second.FindSegment(animeFrame);
    auto start = std::get<0>(segment);
    auto last = std::get<1>(segment);

    // 線形補間.
    auto range = float(last.frame - start.frame);
    XMVECTOR translation = XMLoadFloat3(&start.translation);
    XMVECTOR rotation = XMLoadFloat4(&start.rotation);
    if (range > 0)
    {
      auto rate = float(animeFrame - start.frame) / float(range);
      translation = XMLoadFloat3(&start.translation);
      translation += (XMLoadFloat3(&last.translation) - XMLoadFloat3(&start.translation)) * rate;
      translation += bone->GetInitialTranslation();
      rotation = XMQuaternionNormalize(
        XMQuaternionSlerp(
          XMLoadFloat4(&start.rotation),
          XMLoadFloat4(&last.rotation),
          rate));
    }
    if (range == 0)
    {
      continue;
    }
    bone->SetTranslation(translation);
    bone->SetRotation(rotation);

#if 01
    auto rate = float(animeFrame - start.frame) / float(range);
    XMFLOAT4 bezierK{};
    bezierK.x = InterporateBezier(start.interpX, rate);
    bezierK.y = InterporateBezier(start.interpY, rate);
    bezierK.z = InterporateBezier(start.interpZ, rate);
    bezierK.w = InterporateBezier(start.interpR, rate);

    XMVECTOR k = XMLoadFloat4(&bezierK);
    XMVECTOR sub = XMLoadFloat3(&last.translation) - XMLoadFloat3(&start.translation);
    
    translation = XMLoadFloat3(&start.translation);
    translation += sub * k;
    translation += bone->GetInitialTranslation();
    bone->SetTranslation(translation);

    auto rotA = XMLoadFloat4(&start.rotation);
    auto rotB = XMLoadFloat4(&last.rotation);
    rotation = XMQuaternionSlerp(rotA, rotB, bezierK.w);
    bone->SetRotation(rotation);
#endif
  }
  // 各ボーンの姿勢をセットしたので行列を更新.
  m_model->UpdateMatrices();
}
float Animator::InterporateBezier(const XMFLOAT4& bezier, float x)
{
  float t = 0.5f;
  float ft = fx(bezier.x, bezier.z, t, x);
  for (int i = 0; i < 32; ++i)
  {
    auto dfx = dFx(bezier.x, bezier.z, t);
    t = t - ft / dfx;
    ft = fx(bezier.x, bezier.z, t, x);
  }
  t = std::min(std::max(0.0f, t), 1.0f);
  float dy = funcBezierY(bezier, t);
  return dy;
}

void Animator::UpdateMorthAnimation(uint32_t animeFrame)
{
  for (auto& m : m_morphMap)
  {
    auto name = m.first;
    auto segment = m.second.FindSegment(animeFrame);
    auto start = std::get<0>(segment);
    auto last = std::get<1>(segment);

    auto range = float(last.frame - start.frame);
    auto weight = start.weight;
    if (range > 0)
    {
      auto rate = float(animeFrame - start.frame) / range;
      weight += (last.weight - start.weight) * rate;
    }

    auto index = m_model->GetFaceMorphIndex(name);
    m_model->SetFaceMorphWeight(index, weight);
  }
}

void Animator::Attach(Model* model)
{
  m_model = model;
}

void Animator::UpdateIKchains()
{
  if (m_model == nullptr)
  {
    return;
  }
  auto ikCount = m_model->GetBoneIKCount();
  for (uint32_t i = 0; i < ikCount; ++i)
  {
    SolveIK(m_model->GetBoneIK(i));
  }
}

inline XMVECTOR GetPosition(const XMMATRIX& m)
{
  auto trans = m.r[3];// XMMatrixTranspose(m).r[3];
  XMFLOAT3 vPos;
  XMStoreFloat3(&vPos, trans);
  return trans;
}

void Animator::SolveIK(const PMDBoneIK& boneIk)
{
  auto target = boneIk.GetTarget();
  auto eff = boneIk.GetEffector();

  const auto& chains = boneIk.GetChains();
  for (int ite = 0; ite < boneIk.GetIterationCount(); ++ite)
  {
    for (uint32_t i = 0; i < chains.size(); ++i)
    {
      auto bone = chains[i];
      auto mtxInvBone = XMMatrixInverse(nullptr,bone->GetWorldMatrix());

      // エフェクタとターゲットの位置を、現在ボーンでのローカル空間にする.
      auto effectorPos = XMVector3Transform(GetPosition(eff->GetWorldMatrix()), mtxInvBone); 
      auto targetPos = XMVector3Transform(GetPosition(target->GetWorldMatrix()), mtxInvBone);

      auto len = XMVectorGetX(XMVector3LengthSq(effectorPos - targetPos));
      if (len < 0.0001f)
      {
        return;
      }
      // 現ボーンよりターゲットおよびエフェクタへ向かうベクトルを生成.
      auto vecToEff = XMVector3Normalize(effectorPos);
      auto vecToTarget = XMVector3Normalize(targetPos);

      auto dot = XMVectorGetX(XMVector3Dot(vecToEff, vecToTarget));
      dot = std::min(1.0f, std::max(-1.0f, dot));
      float radian = acosf(dot);
      if (radian < 0.0001f)
        continue;
      auto limitAngle = boneIk.GetAngleWeight();
      radian = std::min(limitAngle, std::max(-limitAngle, radian));

      // 回転軸を求める.
      auto axis = XMVector3Normalize(XMVector3Cross(vecToTarget, vecToEff));

      if (radian < 0.001f)
      {
        continue;
      }

      if (bone->GetName().find("ひざ") != std::string::npos)
      {
        auto rotation = XMQuaternionRotationAxis(axis, radian);
        auto eulerAngle = GetQuaternionToEulerXYZ(rotation);
        eulerAngle.y = 0; eulerAngle.z = 0;

        eulerAngle.x = std::min(XM_PI, std::max(0.002f, eulerAngle.x));
        rotation = MakeQuaternionEuler(eulerAngle);

        rotation = XMQuaternionMultiply(rotation, bone->GetRotation());
        bone->SetRotation(XMQuaternionNormalize(rotation));
      }
      else
      {
        auto rotation = XMQuaternionRotationAxis(axis, radian);
        rotation = XMQuaternionMultiply(rotation, bone->GetRotation());
        bone->SetRotation(XMQuaternionNormalize(rotation));
      }

      // 位置座標更新.
      for (int j = i; j >= 0; --j)
      {
        chains[j]->UpdateWorldMatrix();
      }
      eff->UpdateWorldMatrix();
      target->UpdateWorldMatrix();
    }
  }
}
