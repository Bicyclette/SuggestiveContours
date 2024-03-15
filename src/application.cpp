#include "application.h"

App::App() :
	m_cam(glm::vec3(0.0f, 0.0f, 30.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), static_cast<float>(WIDTH) / static_cast<float>(HEIGHT)),
	m_wireframeShader("shaders/wireframe_vertex.glsl", "shaders/wireframe_geometry.glsl", "shaders/wireframe_fragment.glsl"),
	m_principalDirT1("shaders/principal_directions/T1/vertex.glsl", "shaders/principal_directions/T1/geometry.glsl", "shaders/principal_directions/T1/fragment.glsl"),
	m_principalDirT2("shaders/principal_directions/T2/vertex.glsl", "shaders/principal_directions/T2/geometry.glsl", "shaders/principal_directions/T2/fragment.glsl"),
	m_mesh("assets/stanford_bunny_high_poly.obj")
{
	m_mainShader = std::make_shared<Shader>("shaders/vertex.glsl", "shaders/fragment.glsl");
}