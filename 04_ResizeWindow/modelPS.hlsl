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

float4 main(VSOutput In) : SV_TARGET
{
  float3 toEye = normalize(cameraPos.xyz - In.WorldPos.xyz);
  float3 toLight = normalize(lightPos.xyz);
  float3 halfVector = normalize(toEye + toLight);
  float val = saturate(dot(halfVector, normalize(In.Normal)));
  float specular = pow(val, 50);
  float4 color = In.Color;
  color.rgb += specular;
  return color;
}