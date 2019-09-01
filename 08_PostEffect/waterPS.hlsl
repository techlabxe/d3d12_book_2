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

#define  Iterations  8

float4 main(VSOutput In) : SV_TARGET
{
  float2 uv = In.UV;
  float time = frameCount * 0.01;
  float2 pos = In.Position.xy / windowSize * 12.0 - 20.0;
  float2 tmp = pos;
  float speed2 = speed * 2.0;
  float inten = 0.015;
  float col = 0;

  for (int i = 0; i < Iterations; ++i)
  {
    float t = time * (1.0 - (3.2 / (float(i) + speed)));
    tmp = pos + float2(
      cos(t - tmp.x * ripple) + sin(t + tmp.y * ripple),
      sin(t - tmp.y * ripple) + cos(t + tmp.x * ripple)
      );
    tmp += time;
    col += 1.0 / length(float2(pos.x / (sin(tmp.x + t * speed2) / inten), pos.y / (cos(tmp.y + t * speed2) / inten)));
  }
  col /= float(Iterations);
  col = saturate( 1.5 - sqrt(col) ) ;
  uv += col * distortion;
  return renderedTexture.Sample(texSampler, uv) + col * brightness;
}