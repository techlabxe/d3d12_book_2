#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <map>
#include <algorithm>
#include <DirectXMath.h>


namespace loader
{
    using namespace DirectX;

    class PMDVertex
    {
    public:
        PMDVertex() : m_position(), m_normal(), m_uv(), m_boneNum(), m_boneWeight(), m_edgeFlag() { }

        XMFLOAT3 getPosition() const { return m_position; }
        XMFLOAT3 getNormal() const { return m_normal; }
        XMFLOAT2 getUV() const { return m_uv; }
        uint16_t getBoneIndex(int idx) const { return m_boneNum[idx]; }
        float    getBoneWeight(int idx) const { return (idx == 0 ? m_boneWeight : (100 - m_boneWeight)) / 100.0f; }
        uint8_t  getEdgeFlag() const { return m_edgeFlag; }
    private:
        void load(std::istream& is);
        friend class PMDFile;

        XMFLOAT3    m_position;
        XMFLOAT3    m_normal;
        XMFLOAT2    m_uv;
        std::array<uint16_t,2> m_boneNum;
        uint8_t     m_boneWeight;
        uint8_t     m_edgeFlag;
    };

    class PMDMaterial
    {
    public:
        
        XMFLOAT3 getDiffuse() const { return m_diffuse; }
        XMFLOAT3 getAmbient() const { return m_ambient; }
        XMFLOAT3 getSpecular() const { return m_specular; }
        float   getAlpha() const { return m_alpha; }
        float   getShininess() const { return m_shininess; }
        uint32_t getNumberOfPolygons() const { return m_numberOfPolygons; }

        const std::string& getTexture() const { return m_textureFile; }
        uint8_t getEdgeFlag() const { return m_edgeFlag; }
    private:
        void load(std::istream& is);

        XMFLOAT3 m_diffuse;
        float   m_alpha;
        float   m_shininess;
        XMFLOAT3 m_specular;
        XMFLOAT3 m_ambient;
        uint8_t m_toonID;
        uint8_t m_edgeFlag;
        uint32_t m_numberOfPolygons;
        std::string m_textureFile;

        friend class PMDFile;
    };
    class PMDBone
    {
    public:
        const std::string& getName() const { return m_name; }
        uint16_t getParent() const { return m_parent; }
        
        uint16_t getTarget() const { return m_targetBone; }

        XMFLOAT3 getPosition() const { return m_position; }
    private:
        void load(std::istream& is);

        std::string m_name;
        uint16_t    m_parent;
        uint16_t    m_child;
        uint8_t     m_type;
        uint16_t    m_targetBone;
        XMFLOAT3    m_position;

        friend class PMDFile;
    };

    class PMDIk
    {
    public:
        uint16_t getTargetBoneId() const { return m_boneTarget; }
        uint16_t getBoneEff() const { return m_boneIndex; }
        std::vector<uint16_t> getChains() const { return m_ikBones; }
        uint16_t getIterations() const { return m_numIterations; }
        float    getAngleLimit() const { return m_angleLimit; }
    private:
        void load(std::istream& is);

        uint16_t m_boneIndex;
        uint16_t m_boneTarget;
        uint8_t  m_numChains;
        uint16_t m_numIterations;
        float   m_angleLimit;

        std::vector<uint16_t> m_ikBones;
        friend class PMDFile;
    };

    class PMDFace
    {
    public:
        enum FaceType
        {
            BASE = 0,
            EYEBROW,    /**< まゆ */
            EYE,        /**< 目 */
            LIP,        /**< リップ */
            OTHER,      /**< その他 */
        };
        std::string getName() const { return m_name; }
        FaceType getType() const { return m_faceType; }
        uint32_t getVertexCount() const { return uint32_t(m_faceVertices.size()); }
        uint32_t getIndexCount() const { return uint32_t(m_faceIndices.size()); }

        const XMFLOAT3* getFaceVertices() const { return m_faceVertices.data(); }
        const uint32_t* getFaceIndices() const { return m_faceIndices.data(); }
    private:
        void load(std::istream& is);

        std::string m_name;
        uint32_t    m_numVertices;
        FaceType     m_faceType;

        std::vector<uint32_t> m_faceIndices;
        std::vector<XMFLOAT3> m_faceVertices;
        friend class PMDFile;
    };

    class PMDRigidParam {
    public:
        enum ShapeType {
            SHAPE_SPHERE = 0,
            SHAPE_BOX = 1,
            SHAPE_CAPSULE = 2,
        };
        enum RigidBodyType {
            RIGID_BODY_BONE = 0, // ボーン追従.
            RIGID_BODY_PHYSICS = 1, // 物理演算
            RIGID_BODY_PHYSICS_BONE_CORRECT = 2, // 物理演算(ボーン位置合わせ)
        };

    private:
        void load(std::istream& is);

        std::string m_name;
        uint16_t    m_boneId;
        uint8_t    m_groupId;
        uint16_t    m_groupMask;
        ShapeType   m_shapeType;
        RigidBodyType   m_bodyType;

        float   m_shapeW;
        float   m_shapeH;
        float   m_shapeD;
        XMFLOAT3    m_position;
        XMFLOAT3    m_rotation;
        float   m_weight;
        float   m_attenuationPos;
        float   m_attenuationRot;
        float   m_recoil;
        float   m_friction;

        friend class PMDFile;
    };
    class PMDJointParam {
    public:
    private:
        void load(std::istream& is);

        std::string m_name;
        std::array<uint32_t,2>  m_targetRigidBodies;
        XMFLOAT3    m_position, m_rotation;
        std::array<XMFLOAT3,2>  m_constraintPos;
        std::array<XMFLOAT3,2>  m_constraintRot;
        XMFLOAT3    m_springPos, m_springRot;
        friend class PMDFile;
    };

    class PMDFile
    {
    public:
        PMDFile() {} 
        PMDFile(std::istream& is);

        const std::string& getName() const { return m_name; }
        const std::string& getComment() const { return m_comment; }

        uint32_t getVertexCount() const { return uint32_t(m_vertices.size()); }
        uint32_t getIndexCount() const { return uint32_t(m_indices.size()); }
        uint32_t getMaterialCount() const { return uint32_t(m_materials.size()); }
        uint32_t getBoneCount() const { return uint32_t(m_bones.size()); }
        uint32_t getIkCount() const { return uint32_t(m_iks.size()); }
        uint32_t getFaceCount() const { return uint32_t(m_faces.size()); }
        uint32_t getRigidBodyCount() const { return uint32_t(m_rigidBodies.size()); }
        uint32_t getJointCount() const { return uint32_t(m_joints.size()); }
    
        const PMDVertex& getVertex(int idx) const { return m_vertices[idx]; }
        const PMDVertex* getVertex() const { return m_vertices.data(); }

        uint16_t getIndices(int idx) const { return m_indices[idx]; }
        const uint16_t* getIndices() const { return m_indices.data(); }

        const PMDMaterial& getMaterial(int idx) const { return m_materials[idx]; }
        const PMDBone& getBone(int idx) const { return m_bones[idx]; }
        const PMDIk& getIk(int idx) const { return m_iks[idx]; }
        const PMDFace& getFace(int idx) const { return m_faces[idx]; }
        const PMDFace& getFaceBase() const { auto itr = std::find_if(m_faces.begin(), m_faces.end(), [](const auto & v) { return v.getType() == PMDFace::BASE; }); return *itr; }

    private:
        float m_version;
        std::string m_name;
        std::string m_comment;

        std::vector<PMDVertex> m_vertices;
        std::vector<uint16_t>  m_indices;
        std::vector<PMDMaterial> m_materials;
        std::vector<PMDBone> m_bones;
        std::vector<PMDIk> m_iks;
        std::vector<PMDFace> m_faces;

        std::vector<std::string> m_toonTextures;
        std::vector<PMDRigidParam> m_rigidBodies;
        std::vector<PMDJointParam> m_joints;
    };

    class VMDNode
    {
    public:
        const std::string& getName() const { return m_name; }
        uint32_t getKeyframeNumber() const { return m_keyframe; }
        const XMFLOAT3& getLocation() const { return m_location; }
        const XMFLOAT4& getRotation() const { return m_rotation; }

        XMFLOAT4 getBezierParam(int idx) const;
    private:
        void load(std::istream& is);
        friend class VMDFile;

        uint32_t    m_keyframe;
        XMFLOAT3    m_location;
        XMFLOAT4    m_rotation;
        std::string m_name;
        uint8_t m_interpolation[64];
    };

    class VMDMorph
    {
    public:
      const std::string& getName() const { return m_name; }
      uint32_t getKeyframeNumber() const { return m_keyframe; }

      float getWeight() const { return m_weight; }
    private:
      void load(std::istream& is);
      friend class VMDFile;

      std::string m_name;
      uint32_t m_keyframe;
      float   m_weight;
    };

    class VMDFile
    {
    public:
        VMDFile() = default;

        VMDFile(std::istream& is);

        using VMDNodeKeyframe = std::vector<VMDNode>;
        using VMDAnimationNodeMap = std::map<std::string, VMDNodeKeyframe>;
        using VMDMorphKeyframe = std::vector<VMDMorph>;
        using VMDMorphMap = std::map<std::string, VMDMorphKeyframe>;

        uint32_t getNodeCount() const { return uint32_t(m_nodeNameList.size()); }
        const std::string& getNodeName(int index) const { return m_nodeNameList[index]; }
        uint32_t getMorphCount() const { return uint32_t(m_morphNameList.size()); }
        const std::string& getMorphName(int index) const { return m_morphNameList[index]; }

        VMDNodeKeyframe getKeyframes(const std::string& nodeName) { return m_animationMap[nodeName]; }
        VMDMorphKeyframe getMorphKeyframes(const std::string& morphName) { return m_morphMap[morphName]; }

        uint32_t getKeyframeCount() const { return m_keyframeCount; }
    private:
        const VMDNodeKeyframe* findNode(const std::string& nodeName)
        {
            auto it = m_animationMap.find(nodeName);
            if (it != m_animationMap.end())
            {
                return &(it->second);
            }
            return nullptr;
        }
        uint32_t m_keyframeCount;
        VMDAnimationNodeMap m_animationMap;
        std::vector<std::string> m_nodeNameList;

        VMDMorphMap  m_morphMap;
        std::vector<std::string> m_morphNameList;
    };


    struct memorybuf : std::streambuf {
        memorybuf(char* base, size_t size) : beg(base), end(base+size)
        {
            this->setg(beg,beg,end);
        }
        virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in) override
        {
            if (dir == std::ios_base::cur)
                gbump(int(off));
            else if (dir == std::ios_base::end)
                setg(beg, end + off, end);
            else if (dir == std::ios_base::beg)
                setg(beg, beg + off, end);
            return gptr() - eback();
        }
        virtual pos_type seekpos(std::streampos pos, std::ios_base::openmode mode) override
        {
            return seekoff(pos - pos_type(off_type(0)), std::ios_base::beg, mode);
        }
        char* beg;
        char* end;
    };
    struct memorystream : virtual memorybuf, std::istream {
        memorystream(char* mem, size_t size) :
            memorybuf(mem, size), std::istream(static_cast<std::streambuf*>(this))
        {
        }
    };
}