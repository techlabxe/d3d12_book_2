#pragma once

#include <string>
#include <unordered_map>
#include <algorithm>

#include <DirectXMath.h>

class Model;
class PMDBoneIK;

template<class T>
class Animation
{
public:

  using Segment = std::tuple<T, T>;
  Segment FindSegment(uint32_t frame)
  {
    const T refValue{ frame };
    typename std::vector<T>::iterator first, last;

    last = std::upper_bound(m_keyframes.begin(), m_keyframes.end(),
      refValue, [](const T& a, const T& b) { return a.frame < b.frame; });
    first = last - 1;
    if (last == m_keyframes.end())
    {
      last = first;
    }
    Segment ret;
    std::get<0>(ret) = *first;
    std::get<1>(ret) = *last;
    return ret;
  }

  void SetKeyframes(std::vector<T>& src) { m_keyframes = src; }
private:
  std::vector<T> m_keyframes;
};

struct NodeAnimeFrame
{
  uint32_t frame;
  DirectX::XMFLOAT3 translation;
  DirectX::XMFLOAT4 rotation;
  DirectX::XMFLOAT4 interpX;
  DirectX::XMFLOAT4 interpY;
  DirectX::XMFLOAT4 interpZ;
  DirectX::XMFLOAT4 interpR;

  bool operator<(const NodeAnimeFrame& v) const
  {
    return frame < v.frame;
  }
};
struct MorphAnimeFrame
{
  uint32_t frame;
  float weight;

  bool operator<(const MorphAnimeFrame& v) const
  {
    return frame < v.frame;
  }
};

class NodeAnimation : public Animation<NodeAnimeFrame>
{
public:
  NodeAnimation() { }
};

class MorphAnimation : public Animation<MorphAnimeFrame>
{
public:
  MorphAnimation() = default;
};


class Animator
{
public:
  Animator() : m_model(nullptr), m_framePeriod(0) { }

  void Prepare(const char* filename);
  void Cleanup();

  void UpdateAnimation(uint32_t animeFrame);

  void Attach(Model* model);
private:
  void UpdateNodeAnimation(uint32_t animeFrame);
  void UpdateMorthAnimation(uint32_t animeFrame);
  void UpdateIKchains();
  void SolveIK(const PMDBoneIK&);

  static float InterporateBezier(const DirectX::XMFLOAT4& bezier, float x);

  using NodeAnimationMap = std::unordered_map<std::string, NodeAnimation>;
  using MorphAnimationMap = std::unordered_map<std::string, MorphAnimation>;
  NodeAnimationMap m_nodeMap;
  MorphAnimationMap m_morphMap;
  Model* m_model;

  uint32_t m_framePeriod;
};