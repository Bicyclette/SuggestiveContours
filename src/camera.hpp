#pragma once

#include "GLCommon.h"

struct Camera
{
	Camera(glm::vec3 iPos, glm::vec3 iLookAt, glm::vec3 iUp, float iAspectRatio);
	void rotateView(struct Mouse const & iMouse, struct Viewport const& iViewport);
	void zoomView(struct Mouse const& iMouse);
	void updateView(glm::vec3 iPos, glm::vec3 iLookAt, glm::vec3 iUp, float iAspectRatio);
	glm::vec3 getViewDirection();
	glm::vec3 getRightVector();

	float m_fov;
	glm::mat4 m_proj;
	glm::mat4 m_view;
	glm::vec3 m_position;
	glm::vec3 m_lookAt;
	glm::vec3 m_up;
};