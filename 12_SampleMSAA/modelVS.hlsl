struct VSInput
{
  float4 Position : POSITION;
  float3 Normal : NORMAL;
};
struct VSOutput
{
  float4 Position : SV_POSITION;
  float4 Color : COLOR;
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
  result.Color = saturate(dot(In.Normal, float3(0, 1, 0))) * 0.5 + 0.5;
  return result;
}