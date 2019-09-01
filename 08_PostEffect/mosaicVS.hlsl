struct VSOutput
{
  float4 Position : SV_POSITION;
  float2 UV : TEXCOORD0;
};


VSOutput main( uint id : SV_VERTEXID )
{
  VSOutput result = (VSOutput)0;
  float x = (float)(id % 2) * 2 - 1.0;
  float y = (float)(id / 2) *-2 + 1.0;
  result.Position = float4(x, y, 0, 1);

  float tx = (id % 2);
  float ty = (id / 2);
  result.UV = float2(tx, ty);

  return result;
}