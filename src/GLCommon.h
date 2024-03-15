#pragma once

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define WIDTH 1280
#define HEIGHT 720

struct Viewport
{
	Viewport()
	{
		m_width = WIDTH;
		m_height = HEIGHT;
		m_aspect_ratio = static_cast<float>(m_width) / static_cast<float>(m_height);
	}

	unsigned int m_width;
	unsigned int m_height;
	float m_aspect_ratio;
};

struct Mouse
{
	Mouse()
	{
		m_prevX = 0.0;
		m_prevY = 0.0;
		m_currX = 0.0;
		m_currY = 0.0;
		m_changed = false;;
	}

	double m_prevX;
	double m_prevY;
	double m_currX;
	double m_currY;
	bool m_changed;
};

enum SHADING_MODE
{
	SM_COLOR,
	SM_KG,
	SM_KH,
	SM_SUGGESTIVE_CONTOURS
};

struct UI
{
	bool draw_T1;
	bool draw_T2;
	bool draw_true_contours;
	bool draw_suggestive_contours;
	float object_color[3];
	int shading_mode;
	float max_Kn;
};