#pragma pack_matrix(row_major)

Texture2D shader_texture : register(t0);
SamplerState sample_type : register(s0);

struct PixelShaderInput {
	float4 position : SV_POSITION;
	float2 tex			: TEXCOORD0;
};

float4 main(PixelShaderInput input) : SV_TARGET { 

	float4 texture_colour;
	texture_colour = shader_texture.Sample(sample_type, input.tex);
	return texture_colour;
}