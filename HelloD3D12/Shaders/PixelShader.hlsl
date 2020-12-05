struct PSInput
{
	float4 position : SV_POSITION;
	float4 colour : COLOUR;
};

PSInput main(PSInput psInput) : SV_TARGET
{
	return psInput;
}