struct VSOutput
{
  float4 Position : SV_POSITION;
  float2 UV : TEXCOORD0;
};

SamplerState texSampler : register(s0);
Texture2D renderedTexture : register(t0);


float4 main(VSOutput In) : SV_TARGET
{
  float4 color = renderedTexture.Sample(texSampler, In.UV);
  return color;
}