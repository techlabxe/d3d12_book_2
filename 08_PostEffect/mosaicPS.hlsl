struct VSOutput
{
  float4 Position : SV_POSITION;
  float2 UV : TEXCOORD0;
};

cbuffer EffectParameter : register(b0)
{
  float2 windowSize;
  float  blockSize;
  uint   frameCount;
  float  ripple;
  float  speed;
  float  distortion;
  float  brightness;
}

SamplerState texSampler : register(s0);
Texture2D renderedTexture : register(t0);

float4 main(VSOutput In) : SV_TARGET
{
  float2 uv = In.UV *windowSize;
  uv /= blockSize;
  uv = floor(uv) * blockSize;
  uv /= windowSize;
  
  return renderedTexture.Sample(texSampler, uv);
}