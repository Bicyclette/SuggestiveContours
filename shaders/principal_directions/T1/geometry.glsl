#version 410 core

layout (triangles) in;
layout (line_strip, max_vertices = 12) out;

in VS_OUT
{
	vec4 T1_fwd;
	vec4 T1_bwd;
} gs_in[];

void main()
{
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();
	gl_Position = gs_in[0].T1_fwd;
	EmitVertex();
	EndPrimitive();

	gl_Position = gl_in[0].gl_Position;
	EmitVertex();
	gl_Position = gs_in[0].T1_bwd;
	EmitVertex();
	EndPrimitive();

	gl_Position = gl_in[1].gl_Position;
	EmitVertex();
	gl_Position = gs_in[1].T1_fwd;
	EmitVertex();
	EndPrimitive();

	gl_Position = gl_in[1].gl_Position;
	EmitVertex();
	gl_Position = gs_in[1].T1_bwd;
	EmitVertex();
	EndPrimitive();

	gl_Position = gl_in[2].gl_Position;
	EmitVertex();
	gl_Position = gs_in[2].T1_fwd;
	EmitVertex();
	EndPrimitive();

	gl_Position = gl_in[2].gl_Position;
	EmitVertex();
	gl_Position = gs_in[2].T1_bwd;
	EmitVertex();
	EndPrimitive();
}