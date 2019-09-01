struct VSInput
{
  float4 Position : POSITION;
  float3 Normal : NORMAL;
};
struct VSOutput
{
  float4 Position : SV_POSITION;
  float4 Color : COLOR;
  float3 Normal : NORMAL;
  float4 WorldPos : TEXCOORD0;
};


cbuffer ShaderParameter : register(b0)
{
  float4x4 world;
  float4x4 viewProj;
  float4 lightPos;
  float4 cameraPos;
}

VSOutput main( VSInput In )
{
  VSOutput result = (VSOutput)0;
  float3 lightDir = normalize(lightPos.xyz);
  float4x4 mtxWVP = mul(world, viewProj);
  result.Position = mul(In.Position, mtxWVP);
  result.Color = saturate(dot(In.Normal, lightDir)) * 0.5 + 0.5;
  result.WorldPos = mul(In.Position, world);
  result.Normal = mul(In.Normal, (float3x3)world);
  return result;
}