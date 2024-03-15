#version 410 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent_U;
layout(location = 3) in vec3 tangent_V;
layout (location = 4) in float K1;
layout (location = 5) in float K2;
layout (location = 6) in vec3 T1;
layout (location = 7) in vec3 T2;
layout (location = 8) in mat2 C1; // front slice of C matrix
layout (location = 9) in mat2 C2; // back slice of C matrix

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

out VS_OUT
{
	vec3 fragNormal;
	vec3 fragPos;
	vec3 tangent_U;
	vec3 tangent_V;
	float fK1;
	float fK2;
	vec3 fT1;
	vec3 fT2;
	mat2 C1;
	mat2 C2;
} vs_out;

void main()
{
	gl_Position = proj * view * model * vec4(position, 1.0f);
	vs_out.fragPos = position;
	vs_out.fragNormal = normal;
	vs_out.tangent_U = tangent_U;
	vs_out.tangent_V = tangent_V;
	vs_out.fK1 = K1;
	vs_out.fK2 = K2;
	vs_out.fT1 = T1;
	vs_out.fT2 = T2;
	vs_out.C1 = C1;
	vs_out.C2 = C2;
}