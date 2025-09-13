#pragma pack_matrix(row_major)

struct PixelShaderInput {
	float4 colour : COLOUR;
};

float4 main(PixelShaderInput input) : SV_TARGET { 
	input.colour = mul(input.colour, 1.0f);
	return input.colour; 
}