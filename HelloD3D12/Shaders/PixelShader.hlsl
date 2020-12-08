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

struct PSInput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
};

float3 main(PSInput psInput) : SV_TARGET
{
	return psInput.normal;
}