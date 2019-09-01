struct VSOutput
{
  float4 Position : SV_POSITION;
  float4 ShadowPosition : TEXCOORD0;
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


cbuffer MaterialParameter : register(b2)
{
  float4 diffuse;
  float4 ambient;
  float4 specular;
  uint useTexture;
}

float4 main(VSOutput In) : SV_TARGET
{
  float d = In.ShadowPosition.z / In.ShadowPosition.w;
  return float4(d,d,d,1);
}