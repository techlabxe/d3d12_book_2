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
  float4 branchFlags;
}

static const float3x3 from709to2020 =
{
  { 0.6274040f, 0.3292820f, 0.0433136f },
  { 0.0690970f, 0.9195400f, 0.0113612f },
  { 0.0163916f, 0.0880132f, 0.8955950f }
};
float3 LinearToST2084(float3 val)
{
  float3 st2084 = pow((0.8359375f + 18.8515625f * pow(abs(val), 0.1593017578f)) / (1.0f + 18.6875f * pow(abs(val), 0.1593017578f)), 78.84375f);
  return st2084;
}


float4 main(VSOutput In) : SV_TARGET
{
  float3 toEye = normalize(cameraPos.xyz - In.WorldPos.xyz);
  float3 toLight = normalize(lightPos.xyz);
  float3 halfVector = normalize(toEye + toLight);
  float val = saturate(dot(halfVector, normalize(In.Normal)));

  float shininess = 20;
  float specular = pow(val, shininess);

  float4 color = In.Color;
  color.rgb += specular;

  if (branchFlags.x > 0.5)
  {
    float3 rec2020 = mul(from709to2020, color.rgb);
    float3 hdr10 = LinearToST2084(rec2020 * (80.0 / 10000.0));
    color.rgb = hdr10;
  }

  return color;
}