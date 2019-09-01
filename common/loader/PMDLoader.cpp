#include "PMDloader.h"


#include <pshpack1.h>
namespace loader
{
  namespace rawblock
  {
    using namespace DirectX;
    struct PMDHeader {
      unsigned char magic[3];
      float    version;
      char     name[20];
      char     comment[256];
    };
    struct PMDVertex {
      XMFLOAT3 position;
      XMFLOAT3 normal;
      XMFLOAT2 uv;
      uint16_t boneID[2];
      uint8_t boneWeight;
      uint8_t noEdgeFlag;
    };
    struct PMDMaterial {
      XMFLOAT3 diffuse;
      float alpha;    /**< アルファ */
      float shininess;    /**< Shininess */
      XMFLOAT3 specular;  /**< スペキュラ色 */
      XMFLOAT3 ambient;   /**< アンビエント */
      uint8_t toonID;   /**< ToonIndex. 0xFFでtoon0.bmpを示すらしい */
      uint8_t edgeFlag; /**< エッジフラグ */
      uint32_t numberOfPolygons;   /**< このマテリアルを使用するポリゴン数 */
      char textureFile[20];   /**< テクスチャファイル名 */
    };
    struct PMDBone {
      char name[20];
      uint16_t parentBoneID;/**< 親ボーン */
      uint16_t childBoneID; /**< 子ボーン */
      uint8_t  type;/**< ボーン種別. PMDBoneTypeを参照 */
      uint16_t targetBoneID;/**< ターゲットボーン.種別が(IK影響下,回転影響下,回転連動)の時に使用 */
      XMFLOAT3 position;   /**< ボーン位置(グローバル座標) */
    };
    struct PMDIk {
      uint16_t    destBoneID;   /**< IKボーンの番号(いわゆるエフェクタ) */
      uint16_t    targetBoneID; /**< IKターゲットボーン.　このボーンがdestBoneと同じ位置になるようにしたい. */
      uint8_t     numChains; /**< IK処理に使用するボーンの個数 */
      uint16_t    numIterations; /**< IK処理時に使用する反復回数 */
      float       angleLimit;   /**< 回転制限 */
    };

    struct VMDHeader {
      unsigned char magic[30];
      char modelName[20];
    };


    float readFloat(std::istream& is)
    {
      union { float f; char bin[4]; } data;
      is.read(data.bin, 4);
      return data.f;
    }
    uint8_t readUint8(std::istream& is) { char v; is.read(&v, 1); return uint8_t(v); }
    uint16_t readUint16(std::istream& is)
    {
      union { uint16_t u16; char bin[2]; } data;
      is.read(data.bin, 2);
      return data.u16;
    }
    uint32_t readUint32(std::istream& is)
    {
      union { uint32_t u32; char bin[4]; } data;
      is.read(data.bin, 4);
      return data.u32;
    }
    XMFLOAT2 readFloat2(std::istream& is) { XMFLOAT2 v; v.x = readFloat(is); v.y = readFloat(is); return v; }
    XMFLOAT3 readFloat3(std::istream& is) { XMFLOAT3 v; v.x = readFloat(is); v.y = readFloat(is); v.z = readFloat(is); return v; }
    XMFLOAT4 readFloat4(std::istream& is) { XMFLOAT4 v; v.x = readFloat(is); v.y = readFloat(is); v.z = readFloat(is); v.w = readFloat(is); return v; }

#ifndef USE_LEFTHAND
    XMFLOAT3 flipToRH(XMFLOAT3 v) { v.z *= -1.0f; return v; }
    XMFLOAT4 flipToRH(XMFLOAT4 v) { v.z *= -1.0f; v.w *= -1.0f; return v; }
#else
    XMFLOAT3 flipToRH(XMFLOAT3 v) { return v; }
    XMFLOAT4 flipToRH(XMFLOAT4 v) { return v; }
#endif
  }
}
#include <poppack.h>

namespace loader
{
  using namespace DirectX;

  std::istream& loadFloatFromHex(std::istream& strm, float& v)
  {
    return strm.read(reinterpret_cast<char*>(&v), sizeof(v));
  }

  std::istream& operator>>(std::istream& strm, XMFLOAT2& v)
  {
    loadFloatFromHex(strm, v.x);
    loadFloatFromHex(strm, v.y);
    return strm;
  }

  std::istream& operator>>(std::istream& strm, XMFLOAT3& v)
  {
    strm >> v.x >> v.y >> v.z;
    return strm;
  }

  std::istream& operator>>(std::istream& strm, rawblock::PMDVertex& v)
  {
    strm >> v.position >> v.normal >> v.uv >> v.boneID[0] >> v.boneID[1] >> v.boneWeight >> v.noEdgeFlag;
    return strm;
  }

  void PMDVertex::load(std::istream& is)
  {
    m_position = rawblock::flipToRH(rawblock::readFloat3(is));
    m_normal = rawblock::flipToRH(rawblock::readFloat3(is));
    m_uv = rawblock::readFloat2(is);
    m_boneNum[0] = rawblock::readUint16(is);
    m_boneNum[1] = rawblock::readUint16(is);
    m_boneWeight = rawblock::readUint8(is);
    m_edgeFlag = rawblock::readUint8(is);
  }
  void PMDMaterial::load(std::istream& is)
  {
    m_diffuse = rawblock::readFloat3(is);
    m_alpha = rawblock::readFloat(is);
    m_shininess = rawblock::readFloat(is);
    m_specular = rawblock::readFloat3(is);
    m_ambient = rawblock::readFloat3(is);
    m_toonID = rawblock::readUint8(is);
    m_edgeFlag = rawblock::readUint8(is);
    m_numberOfPolygons = rawblock::readUint32(is);

    char buf[20];
    is.read(buf, sizeof(buf));
    m_textureFile = buf;
  }
  void PMDBone::load(std::istream& is)
  {
    char buf[20];
    is.read(buf, sizeof(buf));
    m_name = buf;
    m_parent = rawblock::readUint16(is);
    m_child = rawblock::readUint16(is);
    m_type = rawblock::readUint8(is);
    m_targetBone = rawblock::readUint16(is);
    m_position = rawblock::flipToRH(rawblock::readFloat3(is));
  }
  void PMDIk::load(std::istream& is)
  {
    m_boneIndex = rawblock::readUint16(is);
    m_boneTarget = rawblock::readUint16(is);
    m_numChains = rawblock::readUint8(is);
    m_numIterations = rawblock::readUint16(is);
    m_angleLimit = rawblock::readFloat(is);
    //m_angleLimit *= DirectX::XM_PI;

    m_ikBones.reserve(m_numChains);
    for (uint32_t i = 0; i < m_numChains; ++i)
    {
      m_ikBones.emplace_back(rawblock::readUint16(is));
    }
  }
  void PMDFace::load(std::istream& is)
  {
    char buf[20];
    is.read(buf, sizeof(buf));
    m_name = buf;
    m_numVertices = rawblock::readUint32(is);
    m_faceType = FaceType(rawblock::readUint8(is));

    m_faceVertices.reserve(m_numVertices);
    m_faceIndices.reserve(m_numVertices);

    for (uint32_t i = 0; i < m_numVertices; ++i)
    {
      m_faceIndices.emplace_back(rawblock::readUint32(is));
      m_faceVertices.emplace_back(rawblock::flipToRH(rawblock::readFloat3(is)));
    }
  }
  void PMDRigidParam::load(std::istream& is)
  {
    char buf[20];
    is.read(buf, sizeof(buf));
    m_name = buf;

    m_boneId = rawblock::readUint16(is);
    m_groupId = rawblock::readUint8(is);
    m_groupMask = rawblock::readUint16(is);
    m_shapeType = ShapeType(rawblock::readUint8(is));
    m_shapeW = rawblock::readFloat(is);
    m_shapeH = rawblock::readFloat(is);
    m_shapeD = rawblock::readFloat(is);
    m_position = rawblock::readFloat3(is);
    m_rotation = rawblock::readFloat3(is);
    m_weight = rawblock::readFloat(is);
    m_attenuationPos = rawblock::readFloat(is);
    m_attenuationRot = rawblock::readFloat(is);
    m_recoil = rawblock::readFloat(is);
    m_friction = rawblock::readFloat(is);
    m_bodyType = RigidBodyType(rawblock::readUint8(is));
  }

  void PMDJointParam::load(std::istream& is)
  {
    char buf[20];
    is.read(buf, sizeof(buf));
    m_name = buf;

    for (auto& v : m_targetRigidBodies)
    {
      v = rawblock::readUint32(is);
    }
    m_position = rawblock::readFloat3(is);
    m_rotation = rawblock::readFloat3(is);
    for (auto& v : m_constraintPos)
    {
      v = rawblock::readFloat3(is);
    }
    for (auto& v : m_constraintRot)
    {
      v = rawblock::readFloat3(is);
    }
    m_springPos = rawblock::readFloat3(is);
    m_springRot = rawblock::readFloat3(is);
  }

  PMDFile::PMDFile(std::istream& is)
  {
    rawblock::PMDHeader header;
    is.read(reinterpret_cast<char*>(&header), sizeof(header));

    m_version = header.version;
    m_name = header.name;
    m_comment = header.comment;

    auto vertexCount = rawblock::readUint32(is);
    m_vertices.resize(vertexCount);
    std::for_each(m_vertices.begin(), m_vertices.end(), [&](auto & v) { v.load(is); });

    auto indexCount = rawblock::readUint32(is);
    m_indices.reserve(indexCount);
    auto polygonCount = indexCount / 3;
    for (uint32_t i = 0; i < polygonCount; ++i)
    {
      auto idx0 = rawblock::readUint16(is);
#ifndef USE_LEFTHAND
      auto idx2 = rawblock::readUint16(is);
      auto idx1 = rawblock::readUint16(is);
#else
      auto idx1 = rawblock::readUint16(is);
      auto idx2 = rawblock::readUint16(is);
#endif
      m_indices.push_back(idx0);
      m_indices.push_back(idx1);
      m_indices.push_back(idx2);
    }

    auto materialCount = rawblock::readUint32(is);
    m_materials.resize(materialCount);
    std::for_each(m_materials.begin(), m_materials.end(), [&](auto & v) { v.load(is); });

    auto boneCount = rawblock::readUint16(is);
    m_bones.resize(boneCount);
    std::for_each(m_bones.begin(), m_bones.end(), [&](auto & v) { v.load(is); });

    auto ikListCount = rawblock::readUint16(is);
    m_iks.resize(ikListCount);
    std::for_each(m_iks.begin(), m_iks.end(), [&](auto & v) {v.load(is); });

    auto faceCount = rawblock::readUint16(is);
    m_faces.resize(faceCount);
    std::for_each(m_faces.begin(), m_faces.end(), [&](auto & v) {v.load(is); });

    // 表情枠. Skip
    auto faceDispCount = rawblock::readUint8(is);
    auto faceDispBlockBytes = faceDispCount * sizeof(uint16_t);
    is.seekg(faceDispBlockBytes, std::ios::cur);

    // ボーン枠名前. Skip
    auto boneDispNameCount = rawblock::readUint8(is);
    auto boneDispNameBlockBytes = boneDispNameCount * sizeof(char[50]);
    is.seekg(boneDispNameBlockBytes, std::ios::cur);

    // ボーン枠. Skip
    auto boneDispCount = rawblock::readUint32(is);
    auto boneDispBlockBytes = boneDispCount * sizeof(char[3]);
    is.seekg(boneDispBlockBytes, std::ios::cur);

    // 英語名ヘッダ. Skip
    auto engNameCount = rawblock::readUint8(is);
    auto engNameBlockBytes = engNameCount * sizeof(char[20 + 256]);
    is.seekg(engNameBlockBytes, std::ios::cur);

    // 英語名ボーン. Skip
    auto engBoneBlockBytes = m_bones.size() * sizeof(char[20]);
    is.seekg(engBoneBlockBytes, std::ios::cur);

    // 英語名表情リスト. Skip
    auto engFaceBlockBytes = (m_faces.size() - 1) * sizeof(char[20]);
    is.seekg(engFaceBlockBytes, std::ios::cur);

    // 英語名ボーン枠. Skip
    auto engBoneDispBlockBytes = boneDispNameCount * sizeof(char[50]);
    is.seekg(engBoneDispBlockBytes, std::ios::cur);

    // トゥーンテクスチャリスト.
    m_toonTextures.resize(10);
    for (int i = 0; i < 10; ++i)
    {
      char fileName[100];
      is.read(fileName, sizeof(fileName));
      m_toonTextures[i] = fileName;
    }

    // 物理演算・剛体.
    auto rigidBodyCount = rawblock::readUint32(is);
    m_rigidBodies.resize(rigidBodyCount);
    std::for_each(m_rigidBodies.begin(), m_rigidBodies.end(), [&](auto & v) { v.load(is); });

    // 物理演算・ジョイント.
    auto jointCount = rawblock::readUint32(is);
    m_joints.resize(jointCount);
    std::for_each(m_joints.begin(), m_joints.end(), [&](auto & v) { v.load(is); });
  }

  template<class T>
  bool KeyframeComparer(const T& a, const T& b)
  {
    return a.getKeyframeNumber() < b.getKeyframeNumber();
  }

  VMDFile::VMDFile(std::istream& is)
  {
    std::vector<VMDNode> nodes;

    rawblock::VMDHeader header{};
    is.read(reinterpret_cast<char*>(header.magic), sizeof(header.magic));
    is.read(header.modelName, sizeof(header.modelName));

    uint32_t dataNum = rawblock::readUint32(is);
    nodes.resize(dataNum);
    std::for_each(nodes.begin(), nodes.end(), [&](auto & v) { v.load(is); });

    std::for_each(nodes.begin(), nodes.end(), [&](auto & v) {
      auto& nodeKeyframes = m_animationMap[v.getName()];
      nodeKeyframes.push_back(v);
      });
    std::for_each(m_animationMap.begin(), m_animationMap.end(), [&](auto & v) {
      std::sort(v.second.begin(), v.second.end(), [&](const auto & a, const auto & b) {
        return a.getKeyframeNumber() < b.getKeyframeNumber();
        });
      });

    m_keyframeCount = 0;
    for (const auto& v : m_animationMap)
    {
      auto itr = v.second.rbegin();

      m_keyframeCount = std::max(m_keyframeCount, itr->getKeyframeNumber());
    }

    for (auto& v : m_animationMap)
    {
      m_nodeNameList.push_back(v.first);
    }

    dataNum = rawblock::readUint32(is);
    std::vector<VMDMorph> morphs;
    morphs.resize(dataNum);
    std::for_each(morphs.begin(), morphs.end(), [&](auto& v) { v.load(is); });
    std::for_each(morphs.begin(), morphs.end(), [&](auto& v) {
      auto& morphKeyframes = m_morphMap[v.getName()];
      morphKeyframes.push_back(v);
      });
    std::for_each(m_morphMap.begin(), m_morphMap.end(), [&](auto& v) {
      std::sort(v.second.begin(), v.second.end(), KeyframeComparer<VMDMorph>);
      }
    );
    for (auto& v : m_morphMap)
    {
      m_morphNameList.push_back(v.first);
    }
  }

  void VMDNode::load(std::istream& is)
  {
    char nameBuf[15];
    is.read(nameBuf, sizeof(nameBuf));
    m_name = nameBuf;
    m_keyframe = rawblock::readUint32(is);
    m_location = rawblock::flipToRH(rawblock::readFloat3(is));
    m_rotation = rawblock::flipToRH(rawblock::readFloat4(is));

    is.read(reinterpret_cast<char*>(m_interpolation), sizeof(m_interpolation));
  }

  DirectX::XMFLOAT4 VMDNode::getBezierParam(int idx) const
  {
    XMFLOAT4 ret;
    ret.x = float(m_interpolation[4 * 0 + idx]) / 127.0f;
    ret.y = float(m_interpolation[4 * 1 + idx]) / 127.0f;
    ret.z = float(m_interpolation[4 * 2 + idx]) / 127.0f;
    ret.w = float(m_interpolation[4 * 3 + idx]) / 127.0f;
    return ret;
  }

  void VMDMorph::load(std::istream& is)
  {
    char nameBuf[15];
    is.read(nameBuf, sizeof(nameBuf));
    m_name = nameBuf;
    m_keyframe = rawblock::readUint32(is);
    m_weight = rawblock::readFloat(is);
  }
}