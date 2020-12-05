struct VSOutput
{
	float4 position : SV_POSITION;
	float4 colour : COLOUR;
};

VSOutput main(float2 position : POSITION, float4 colour : COLOUR)
{
	VSOutput vso;
	vso.position = float4(position.x, position.y, 0.0f, 1.0f);
	vso.colour = colour;
	return vso;
}