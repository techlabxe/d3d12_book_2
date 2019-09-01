struct VSInput
{
  float4 Position : POSITION;
  float3 Normal : NORMAL;
  float4 OffsetPos : WORLD_POS;
  float4 Color : BASE_COLOR;
};
struct VSOutput
{
  float4 Position : SV_POSITION;
  float4 Color : COLOR;
};


cbuffer ShaderParameter : register(b0)
{
  float4x4 world;
  float4x4 view;
  float4x4 proj;
}

VSOutput main( VSInput In )
{
  VSOutput result = (VSOutput)0;
  float4 pos = In.Position;
  pos.xyz += In.OffsetPos.xyz;
  float4x4 mtxWVP = mul(world, mul(view, proj));
  result.Position = mul(pos, mtxWVP);
  result.Color = saturate(dot(In.Normal, float3(0, 1, 0))) * 0.5 + 0.5;
  result.Color.a = 1;
  result.Color *= In.Color;
  return result;
}