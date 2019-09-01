struct VSInput
{
  float4 Position : POSITION;
  float3 Normal : NORMAL;
  uint InstanceID : SV_InstanceID;
};
struct VSOutput
{
  float4 Position : SV_POSITION;
  float4 Color : COLOR;
};

struct InstanceData
{
  float4x4 world;
  float4   color;
};

cbuffer ShaderParameter : register(b0)
{
  float4x4 view;
  float4x4 proj;
}
cbuffer InstanceParameter : register(b1)
{
  InstanceData data[500];
}

VSOutput main( VSInput In )
{
  VSOutput result = (VSOutput)0;
  
  uint index = In.InstanceID;
  float4x4 world = data[index].world;
  float4x4 mtxWVP = mul(world, mul(view, proj));
  result.Position = mul(In.Position, mtxWVP);
  result.Color = saturate(dot(In.Normal, float3(0, 1, 0))) * 0.5 + 0.5;
  result.Color.a = 1;
  result.Color *= data[index].color;
  return result;
}