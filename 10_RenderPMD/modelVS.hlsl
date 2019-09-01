struct VSInput
{
  float4 Position : POSITION;
  float3 Normal : NORMAL;
  float2 UV : TEXCOORD0;
  uint2 BlendIndices : BLENDINDICES;
  float2 BlendWeights: BLENDWEIGHTS;
  uint   EdgeFlag : EDGEFLAG;
};

struct VSOutput
{
  float4 Position : SV_POSITION;
  float2 UV : TEXCOORD0;
  float3 Normal : TEXCOORD1;
  float4 WorldPosition  : TEXCOORD2;

  float4 ShadowPos : POSITION_LIGHTSPACE;
  float4 ShadowPosUV : SHADOWMAP_UV;
};

cbuffer SceneParameter : register(b0)
{
  float4x4 view;
  float4x4 proj;
  float4   lightDirection;
  float4   cameraPos;
  float4   outlineColor;

  float4x4 lightViewProj;
  float4x4 lightViewProjBias;
}


cbuffer BoneParameter : register(b1)
{
  float4x4 boneMatrices[256];
}

float4 TransformPosition(float4 inPosition, VSInput In)
{
  float4 pos = 0;
  uint indices[2] = (uint[2])In.BlendIndices;
  float weights[2] = (float[2])In.BlendWeights;
  for (int i = 0; i < 2; ++i)
  {
    float4x4 mtx = boneMatrices[indices[i]];
    float w = weights[i];
    pos += mul(inPosition, mtx) * w;
  }
  pos.w = 1;
  return pos;
}
float3 TransformNormal(float3 inNormal, VSInput In)
{
  float3 normal = 0;
  uint indices[2] = (uint[2])In.BlendIndices;
  float weights[2] = (float[2])In.BlendWeights;
  for (int i = 0; i < 2; ++i)
  {
    float4x4 mtx = boneMatrices[indices[i]];
    float w = weights[i];
    normal += mul(inNormal, (float3x3)mtx) * w;
  }
  return normal;
}

VSOutput main( VSInput In )
{
  VSOutput result = (VSOutput)0;
  float4x4 mtxVP = mul(view, proj);

  float4 pos = TransformPosition(In.Position, In);
  float3 nrm = TransformNormal(In.Normal, In);

  result.Position = mul(pos, mtxVP);
  result.Normal = normalize(nrm);
  result.UV = In.UV;
  result.WorldPosition = pos;
  result.ShadowPos = mul(pos, lightViewProj);
  result.ShadowPosUV = mul(pos, lightViewProjBias);
  return result;
}