struct VSOutput
{
  float4 Position : SV_POSITION;
  float2 UV : TEXCOORD0;
  float3 Normal : TEXCOORD1;
  float4 WorldPosition  : TEXCOORD2;

  float4 ShadowPos : POSITION_LIGHTSPACE;
  float4 ShadowPosUV : SHADOWMAP_UV;
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

Texture2D diffuseTexture : register(t0);
SamplerState diffuseSampler : register(s0);

Texture2D shadowTexture : register(t1);
SamplerState shadowSampler : register(s1);


float4 main(VSOutput In) : SV_TARGET
{
  float3 normal = normalize(In.Normal);
  float3 lightdir = normalize(lightDirection.xyz);

  float4 color = diffuse;
  if (useTexture != 0)
  {
    color *= diffuseTexture.Sample(diffuseSampler, In.UV);
  }
  float3 baseColor = color.rgb;

  //float lmb = (0.5 * dot(lightdir, normal) + 0.5); // half lambert
  float lmb = saturate(dot(lightdir, normal)); // lambert
  color.rgb = baseColor * lmb;
  color.rgb += baseColor * ambient.xyz;

  float3 toEyeDirection = normalize(cameraPos.xyz - In.WorldPosition.xyz);
  float3 vH = normalize(toEyeDirection + lightdir);
  float spc = pow(saturate(dot(normal, vH)), specular.w);
  color.xyz += spc * specular.rgb;

  float4 uv = In.ShadowPosUV / In.ShadowPosUV.w;
  float depthFromLight = shadowTexture.Sample(shadowSampler, uv).r +0.001;

  float z = In.ShadowPos.z / In.ShadowPos.w;
  if (depthFromLight < z)
  {
    // ‰e‚É‚È‚Á‚Ä‚¢‚é.
    color.rgb *= 0.7;
  }

  return color;
}