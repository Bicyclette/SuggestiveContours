#pragma once

#include "mesh.hpp"

struct FBO
{
	GLuint m_fbo;
	GLuint m_texture;
	GLuint m_rbo;

	~FBO()
	{
		glDeleteFramebuffers(1, &m_fbo);
		glDeleteTextures(1, &m_texture);
		glDeleteRenderbuffers(1, &m_rbo);
	}
};

struct App
{
	App();

	Camera m_cam;
	std::shared_ptr<Shader> m_mainShader;
	Shader m_wireframeShader;
	Shader m_principalDirT1;
	Shader m_principalDirT2;
	Mesh m_mesh;
	struct Mouse m_mouse;
	struct Viewport m_viewport;
};