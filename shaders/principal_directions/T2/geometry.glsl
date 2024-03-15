#version 410 core

layout (triangles) in;
layout (line_strip, max_vertices = 12) out;

in VS_OUT
{
	vec4 T2_fwd;
	vec4 T2_bwd;
} gs_in[];

void main()
{
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();
	gl_Position = gs_in[0].T2_fwd;
	EmitVertex();
	EndPrimitive();

	gl_Position = gl_in[0].gl_Position;
	EmitVertex();
	gl_Position = gs_in[0].T2_bwd;
	EmitVertex();
	EndPrimitive();

	gl_Position = gl_in[1].gl_Position;
	EmitVertex();
	gl_Position = gs_in[1].T2_fwd;
	EmitVertex();
	EndPrimitive();

	gl_Position = gl_in[1].gl_Position;
	EmitVertex();
	gl_Position = gs_in[1].T2_bwd;
	EmitVertex();
	EndPrimitive();

	gl_Position = gl_in[2].gl_Position;
	EmitVertex();
	gl_Position = gs_in[2].T2_fwd;
	EmitVertex();
	EndPrimitive();

	gl_Position = gl_in[2].gl_Position;
	EmitVertex();
	gl_Position = gs_in[2].T2_bwd;
	EmitVertex();
	EndPrimitive();
}