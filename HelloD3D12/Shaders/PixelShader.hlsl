#ifndef NUM_DIR_LIGHTS
	#define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
	#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

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

// Linear attentuation factor which applies to point and spot lights
float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
	// Linear falloff
	return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

// Schlick gives an approximation to Fresnel reflectance (see pg. 233 "Real-Time Rendering")
// R0 = ( (n-1) / (n+1) )^2, where n is the index of refraction.
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
	float cosIncidentAngle = saturate(dot(normal, lightVec));

	float f0 = 1.0f - cosIncidentAngle;

	float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

	return reflectPercent;
}

struct Material
{
	float4 DiffuseAlbedo;
	float3 FresnelR0;

	// Shininess is inverse of roughness: 1 - roughness
	float Shininess;
};

float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
	// Derive m from the shininess, which is derived from the roughness.
	const float m = mat.Shininess * 256.0f;
	float3 halfVec = normalize(toEye + lightVec);

	float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
	float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

	// Our spec formula goes out [0,1] range, but we are doing 
	// LDR rendering. So scale it down a bit.

	float3 specAlbedo = fresnelFactor * roughnessFactor;
	specAlbedo = specAlbedo / (specAlbedo + 1.0f);

	return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;

}

float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
	// The light vector aims opposite the direction the light rays travel.
	float3 lightVec = -L.Direction;

	// Scale light down by Lmabert's cosine law.
	float ndotl = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = L.Strength * ndotl;

	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
	// The vector from the surface to the light.
	float3 lightVec = L.Position - pos;

	// The distance from surface to light.
	float d = length(lightVec);

	// Range test.
	if (d > L.FalloffEnd)
		return 0.0f;


	// Normalize the light vector.
	lightVec /= d;

	// Scale light down by Lambert's cosine law.
	float ndotl = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = L.Strength * ndotl;

	// Attentuate light by distance.
	float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	lightStrength *= att;

	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
	// The vector from the surface to the light.
	float3 lightVec = L.Position - pos;

	// The distance from surface to light.
	float d = length(lightVec);

	// Range test.
	if (d > L.FalloffEnd)
	{
		return 0.0f;
	}

	// Normalize the light vector.
	lightVec /= d;

	// Scale light down by Lambert's cosine law
	float ndotl = (max(dot(lightVec, normal), 0.0f));
	float3 lightStrength = L.Strength * ndotl;

	// Attentuate light by distance
	float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	lightStrength *= att;

	// Scale by spotlight
	float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
	lightStrength *= spotFactor;

	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float4 ComputeLighting(Light gLights[MaxLights], Material mat, float3 pos, float3 normal, float3 toEye, float3 shadowFactor)
{
	float3 result = 0.0f;

	int i = 0;

	#if (NUM_DIR_LIGHTS > 0)
	for (i = 0; i < NUM_DIR_LIGHTS; i++)
	{
		result += shadowFactor * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
	}
	#endif

	#if (NUM_POINT_LIGHTS > 0)
	for (i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i++)
	{
		result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
	}
	#endif

	#if (NUM_SPOT_LIGHTS > 0)
	for (i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; i++)
	{
		result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
	}
	#endif

	return float4(result, 0.0f);
}

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
	float padding;
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

Texture2D gDiffuseMap : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamLinearWrap : register(s1);

struct PSInput
{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC : TEXCOORD;
};

float4 main(PSInput psInput) : SV_TARGET
{
	
	float4 diffuseAlbedo = gDiffuseMap.Sample(gsamPointWrap, psInput.TexC) * gDiffuseAlbedo;

	// Interpolating normal can unnormalize it,
	// so renormalize it.
	psInput.NormalW = normalize(psInput.NormalW);

	// Vector from point being lit to eye.
	float3 toEyeW = normalize(gEyePosW - psInput.PosW);

	// Indirect lighting.
	float4 ambient = gAmbientLight * diffuseAlbedo;

	// Direct lighting.
	const float shininess = 1.0f - gRoughness;
	Material mat = { diffuseAlbedo, gFresnelR0, shininess };
	float3 shadowFactor = 1.0f;
	float4 directLight = ComputeLighting(gLights, mat, psInput.PosW, psInput.NormalW, toEyeW, shadowFactor);

	float4 litColour = ambient + directLight;

	//Common convention to take alpha from diffuse material.
	litColour.a = diffuseAlbedo.a;
	return litColour;
	
	//return gDiffuseMap.Sample(gsamPointWrap, psInput.TexC);
}