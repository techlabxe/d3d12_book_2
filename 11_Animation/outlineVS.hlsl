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

VSOutput main( VSInput In )
{
  VSOutput result = (VSOutput)0;
  float4x4 mtxVP = mul(view, proj);

  float4 pos = TransformPosition(In.Position, In);
  result.Position = mul(pos, mtxVP);
  if (In.EdgeFlag == 0)
  {
    float4 basePos = result.Position;
    float4 offseted = float4(In.Position.xyz + In.Normal, 1);
    float4 outlinePos = mul(TransformPosition(offseted, In), mtxVP);
    
    float4 vec = normalize(outlinePos - basePos);
    result.Position = basePos + vec * 0.004 * basePos.w;
  }

  return result;
}