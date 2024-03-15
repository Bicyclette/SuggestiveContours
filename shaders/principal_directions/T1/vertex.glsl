#version 410 core

layout (location = 0) in vec3 vPos;
layout (location = 6) in vec3 vT1;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

out VS_OUT
{
	vec4 T1_fwd;
	vec4 T1_bwd;
} vs_out;

void main()
{
	const float scale = 0.35f;
	gl_Position = proj * view * model * vec4(vPos, 1.0f);
	vs_out.T1_fwd = proj * view * model * vec4(vPos + (vT1 * scale), 1.0f);
	vs_out.T1_bwd = proj * view * model * vec4(vPos - (vT1 * scale), 1.0f);
}