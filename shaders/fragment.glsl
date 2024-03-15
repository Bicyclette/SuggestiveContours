#version 410 core

struct PointLight
{
	vec3 position;
	vec3 color;
};

in VS_OUT
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
} fs_in;

const vec3 gradient_color = vec3(1.0f);

out vec4 color;

uniform float minKg;
uniform float maxKg;
uniform float minH;
uniform float maxH;

uniform vec3 objectColor;
uniform vec3 viewPosition;
uniform bool draw_true_contours;
uniform bool draw_suggestive_contours;
uniform bool draw_strong_suggestive_contours;

uniform int shading_mode; // 0: color, 1: gaussian curvature, 2: mean curvature
uniform float max_Kn; // accept from 0.0f to this value

vec3 project_viewDir_on_tangent_plane()
{
	vec3 viewDir = viewPosition - fs_in.fragPos;
	vec3 u = normalize(fs_in.tangent_U);
	vec3 v = normalize(fs_in.tangent_V);
	float amount_of_u = dot(u, viewDir);
	float amount_of_v = dot(v, viewDir);
	return (amount_of_u * u) + (amount_of_v * v);
}

float compute_angle_between_vectors(vec3 t1, vec3 w)
{
	return acos(dot(normalize(t1), normalize(w)));
}

vec3 gradient_gaussian_curvature()
{
	float Kg = fs_in.fK1 * fs_in.fK2;
	float value = (Kg + abs(minKg)) / (abs(minKg) + abs(maxKg));
	return vec3(value * gradient_color);
}

vec3 gradient_mean_curvature()
{
	float H = (fs_in.fK1 + fs_in.fK2) / 2.0f;
	float value = (H + abs(minH)) / (abs(minH) + abs(maxH));
	return vec3(value * gradient_color);
}

bool true_contour()
{
	vec3 viewDir = normalize(viewPosition - fs_in.fragPos);
	float c = dot(fs_in.fragNormal, viewDir);
	if(c >= 0.0f && c <= 0.25f)
	{
		return true;
	}
	return false;
}

float derivative_radial_curvature_along_w(vec3 iW)
{
	vec2 w = iW.xy;
	mat2 C1 = fs_in.C1;
	mat2 C2 = fs_in.C2;
	
	vec2 col1 = C1 * w;
	vec2 col2 = C2 * w;

	return dot(vec2(col1.x * w.x + col2.x * w.y, col1.y * w.x + col2.y * w.y), w);
}

bool keep_fragment(float DwKn_mag)
{
	vec3 N = normalize(fs_in.fragNormal);
	vec3 viewDir = viewPosition - fs_in.fragPos;
	vec3 viewDirUnit = normalize(viewPosition - fs_in.fragPos);
	float np_vp = dot(N, viewDir);
	float angle_N_viewDir = acos(dot(N, viewDirUnit)); // degrees

	float reject_angle = np_vp / length(viewDir);

	float magnitude = 0.35f;

	if(angle_N_viewDir > reject_angle && DwKn_mag > magnitude)
	{
		return true;
	}
	return false;
}

void main()
{
	struct PointLight light;
	light.position = vec3(10.0f, 10.0f, 10.0f);
	light.color = vec3(1.0f, 1.0f, 1.0f);

	if(shading_mode == 0)
	{
		color = vec4(objectColor, 1.0f);
	}
	else if(shading_mode == 1)
	{
		color = vec4(gradient_gaussian_curvature(), 1.0f);
	}
	else if(shading_mode == 2)
	{
		color = vec4(gradient_mean_curvature(), 1.0f);
	}
	else
	{
		vec3 w = project_viewDir_on_tangent_plane();
		float theta = compute_angle_between_vectors(w, fs_in.fT1);
		float Kn = fs_in.fK1 * pow(cos(theta), 2.0f) + fs_in.fK2 * pow(sin(theta), 2.0f);
		float DwKn = derivative_radial_curvature_along_w(w);
		float derivative_magnitude = DwKn / length(w);

		if(draw_true_contours && !draw_suggestive_contours)
		{
			if(true_contour())
			{
				color = vec4(vec3(0.0f), 1.0f);
			}
			else
			{
				color = vec4(objectColor, 1.0f);
			}
		}
		else if(!draw_true_contours && draw_suggestive_contours)
		{
			if((Kn > 0.0f && Kn <= max_Kn) && (DwKn > 0.0f))
			{
				if(keep_fragment(derivative_magnitude))
				{
					color = vec4(vec3(0.0f), 1.0f);
				}
				else
				{
					color = vec4(objectColor, 1.0f);
				}
			}
			else
			{
				color = vec4(objectColor, 1.0f);
			}
		}
		else if(draw_true_contours && draw_suggestive_contours)
		{
			if(true_contour())
			{
				color = vec4(vec3(0.0f), 1.0f);
			}

			else if((Kn >= 0.0f && Kn <= max_Kn) && (DwKn > 0.0f))
			{
				if(keep_fragment(derivative_magnitude))
				{
					color = vec4(vec3(0.0f), 1.0f);
				}
				else
				{
					color = vec4(objectColor, 1.0f);
				}
			}
			else
			{
				color = vec4(objectColor, 1.0f);
			}
		}
		else
		{
			color = vec4(objectColor, 1.0f);
		}
	}
}