#define MaxLights 16

struct Light
{
	float3 Strength; // Light colour
	float FalloffStart; // point/spot light only
	float3 Direction; // Directional/spot light only
	float FalloffEnd; // point/spot light only
	float3 Position; // point/spot light only
	float SpotPower; // spot light only
};

cbuffer ConstantBuffer : register(b0)
{
	matrix gWorld;
};

cbuffer cbMaterial : register(b1)
{
	float4 gDiffuseAlbedo;
	float3 gFresnelR0;
	float gRoughness;
	float4x4 gMatTransform;
}

cbuffer cbPass : register (b2)
{
// See if we can get by without using gView for now
//	matrix gView;
	float4 gAmbientLight;

	// Indices[0, NUM_DIR_LIGHTS] are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS] are
	// point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, 
	// NUM_DIR_LIGHTS+NUM_POINT_LIGHTS+NUM_SPOT_LIGHTS]
	// are spot lights for a maximum of MaxLights per object.
	Light gLights[MaxLights];
}

struct VSOutput
{
	float4 position : SV_POSITION;
	float4 colour : COLOUR;
};

VSOutput main(float3 position : POSITION, float4 colour : COLOUR)
{

	// for now just use gWorld, see if we need to use gWorld and then multiply that by gView
	VSOutput vso;
	vso.position = mul(float4(position, 1.0f), gWorld);
//	vso.position = float4(position, 1.0f);
	vso.colour = colour;
	return vso;
}