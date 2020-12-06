cbuffer ConstantBuffer : register(b0)
{
	matrix transform;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float4 colour : COLOUR;
};

VSOutput main(float3 position : POSITION, float4 colour : COLOUR)
{
	VSOutput vso;
	


	vso.position = mul(float4(position.x, position.y, position.z, 1.0f), transform);
//	position = float4(position, 1.0f);
	vso.colour = colour;
	return vso;
}