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
	float3 gEyePosW;
	// TODO: Look out for padding here
	matrix gView;
	matrix gProj;
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

struct VSInput
{
	float3 PosL : POSITION;
	float3 NormalL : NORMAL;
};

struct VSOutput
{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
};

VSOutput main(VSInput vsInput)
{

	VSOutput vsOut = (VSOutput)0.0f;

	// Transform to world space.
	float4 posW = mul(float4(vsInput.PosL, 1.0f), gWorld);
	vsOut.PosW = posW.xyz;

	// Assumes nonuniform scaling; otherwise, need to 
	// use inverse-transpose of world matrix.
	vsOut.NormalW = mul(vsInput.NormalL, (float3x3)gWorld);

	vsOut.PosH = mul(posW, gView);
//	vsOut.PosH = mul(vsOut.PosH, gProj);
	return vsOut;
}