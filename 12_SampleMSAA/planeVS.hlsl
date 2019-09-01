struct VSInput
{
  float4 Position : POSITION;
  float2 UV : TEXCOORD0;
};
struct VSOutput
{
  float4 Position : SV_POSITION;
  float2 UV : TEXCOORD0;
};


cbuffer ShaderParameter : register(b0)
{
  float4x4 world;
  float4x4 viewProj;
}

VSOutput main( VSInput In )
{
  VSOutput result = (VSOutput)0;
  float4x4 mtxWVP = mul(world, viewProj);
  result.Position = mul(In.Position, mtxWVP);
  result.UV = In.UV;
  return result;
}