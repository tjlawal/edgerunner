// We are row-major by default
#pragma pack_matrix(row_major)

cbuffer PerApplication 	: register (b0) { matrix projection_matrix; } // Updated once at application startup
cbuffer PerFrame 				: register (b1) { matrix view_matrix; }				// Updated every frame
cbuffer PerObject 			: register (b2) { matrix world_matrix; }			// Updated per object

// Application shader variables
struct AppData {
	float4 position : POSITION;
	float4 colour   : COLOUR;
};

struct VertexShaderOutput {
	float4 colour   : COLOUR;
	float4 position : SV_POSITION;
};

VertexShaderOutput main(AppData input) {
	VertexShaderOutput output;

	input.position.w = 1.0f;
	output.position = mul(input.position, world_matrix);
	output.position = mul(output.position, view_matrix);
	output.position = mul(output.position, projection_matrix);

	output.colour = input.colour;
	return output;
}