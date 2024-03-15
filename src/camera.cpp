#include "camera.hpp"

Camera::Camera(glm::vec3 iPos, glm::vec3 iLookAt, glm::vec3 iUp, float iAspectRatio)
{
	m_fov = 45.0f;
	m_position = iPos;
	m_lookAt = iLookAt;
	m_up = iUp;
	m_view = glm::lookAt(m_position, m_lookAt, m_up);
	m_proj = glm::perspective(45.0f, iAspectRatio, 0.1f, 100.0f);
}

void Camera::rotateView(struct Mouse const& iMouse, struct Viewport const& iViewport)
{
	glm::vec4 eye = glm::vec4(m_position, 1.0f);
	glm::vec4 center = glm::vec4(m_lookAt, 1.0f);

	float dtx = (2 * M_PI) / iViewport.m_width;
	float dty = M_PI / iViewport.m_height;
	float azimuth = (iMouse.m_prevX - iMouse.m_currX) * dtx;
	float zenith = (iMouse.m_prevY - iMouse.m_currY) * dty;

	glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), azimuth, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), zenith, getRightVector());
	eye = center + Rx * (eye - center);
	m_position = center + Ry * (eye - center);

	updateView(m_position, m_lookAt, glm::vec3(0.0f, 1.0f, 0.0f), iViewport.m_aspect_ratio);
}

void Camera::zoomView(struct Mouse const& iMouse)
{

}

void Camera::updateView(glm::vec3 iPos, glm::vec3 iLookAt, glm::vec3 iUp, float iAspectRatio)
{
	m_position = iPos;
	m_lookAt = iLookAt;
	m_up = iUp;
	m_view = glm::lookAt(m_position, m_lookAt, m_up);
	m_proj = glm::perspective(m_fov, iAspectRatio, 0.1f, 100.0f);
}

glm::vec3 Camera::getViewDirection()
{
	return -glm::transpose(m_view)[2];
}

glm::vec3 Camera::getRightVector()
{
	return glm::transpose(m_view)[0];
}