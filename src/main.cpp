#include "application.h"

std::shared_ptr<struct App> g_app;
struct UI g_ui;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	g_app->m_viewport.m_width = width;
	g_app->m_viewport.m_height = height;
	g_app->m_viewport.m_aspect_ratio = static_cast<float>(g_app->m_viewport.m_width) / static_cast<float>(g_app->m_viewport.m_height);
	g_app->m_cam.updateView(g_app->m_cam.m_position, g_app->m_cam.m_lookAt, glm::vec3(0.0f, 1.0f, 0.0f), g_app->m_viewport.m_aspect_ratio);
}

void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, true);
	}

	if (glfwGetKey(window, GLFW_KEY_F12) == GLFW_PRESS) // recompile fragment shader
	{
		g_app->m_mainShader = std::make_shared<Shader>("shaders/vertex.glsl", "shaders/fragment.glsl");
	}

	// camera related
	double xpos;
	double ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	g_app->m_mouse.m_prevX = g_app->m_mouse.m_currX;
	g_app->m_mouse.m_prevY = g_app->m_mouse.m_currY;
	g_app->m_mouse.m_currX = xpos;
	g_app->m_mouse.m_currY = ypos;
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_3) == GLFW_PRESS)
	{
		g_app->m_mouse.m_changed = true;
	}
}

void draw_UI()
{
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(300, 60));
	ImGui::Begin("Object base color");
	ImGui::ColorEdit3("Color", g_ui.object_color);
	ImGui::End();
	
	ImGui::SetNextWindowPos(ImVec2(0, 60));
	ImGui::SetNextWindowSize(ImVec2(300, 60));
	ImGui::Begin("Taubin smoothing");
	if (ImGui::Button("process"))
	{
		g_app->m_mesh.taubin_smoothing();
	}
	ImGui::End();
	
	ImGui::SetNextWindowPos(ImVec2(0, 120));
	ImGui::SetNextWindowSize(ImVec2(300, 130));
	ImGui::Begin("Shading mode");
	ImGui::RadioButton("Base color + wireframe", &g_ui.shading_mode, SM_COLOR);
	ImGui::RadioButton("Gaussian Curvature", &g_ui.shading_mode, SM_KG);
	ImGui::RadioButton("Mean Curvature", &g_ui.shading_mode, SM_KH);
	ImGui::RadioButton("Suggestive Contours", &g_ui.shading_mode, SM_SUGGESTIVE_CONTOURS);
	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(0, 250));
	ImGui::SetNextWindowSize(ImVec2(300, 100));
	ImGui::Begin("Suggestive Contours settings");
	if (g_ui.shading_mode == SM_SUGGESTIVE_CONTOURS)
	{
		ImGui::SliderFloat("max Kn", &g_ui.max_Kn, 0.0f, 0.2f);
		ImGui::Checkbox("Draw true contours", &g_ui.draw_true_contours);
		ImGui::Checkbox("Draw suggestive_contours", &g_ui.draw_suggestive_contours);
	}
	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(0, 350));
	ImGui::SetNextWindowSize(ImVec2(300, 100));
	ImGui::Begin("Principal curvatures' directions");
	ImGui::Checkbox("Draw principal direction T1 (max)", &g_ui.draw_T1);
	ImGui::Checkbox("Draw principal direction T2 (min)", &g_ui.draw_T2);
	ImGui::End();
}

void draw_mesh()
{
	// render mesh
	glBindVertexArray(g_app->m_mesh.m_vao);

	glUseProgram(g_app->m_mainShader->m_program);
	g_app->m_mainShader->setMat4f("model", g_app->m_mesh.m_model);
	g_app->m_mainShader->setMat4f("view", g_app->m_cam.m_view);
	g_app->m_mainShader->setMat4f("proj", g_app->m_cam.m_proj);
	g_app->m_mainShader->setVec3f("viewPosition", g_app->m_cam.m_position);
	g_app->m_mainShader->setVec3f("objectColor", glm::vec3(g_ui.object_color[0], g_ui.object_color[1], g_ui.object_color[2]));
	g_app->m_mainShader->setInt("shading_mode", g_ui.shading_mode);
	if (g_ui.draw_true_contours)
	{
		g_app->m_mainShader->setBool("draw_true_contours", true);
	}
	else
	{
		g_app->m_mainShader->setBool("draw_true_contours", false);
	}
	if (g_ui.draw_suggestive_contours)
	{
		g_app->m_mainShader->setBool("draw_suggestive_contours", true);
	}
	else
	{
		g_app->m_mainShader->setBool("draw_suggestive_contours", false);
	}
	g_app->m_mainShader->setFloat("max_Kn", g_ui.max_Kn);
	g_app->m_mainShader->setFloat("minKg", g_app->m_mesh.m_geom.m_minKg);
	g_app->m_mainShader->setFloat("maxKg", g_app->m_mesh.m_geom.m_maxKg);
	g_app->m_mainShader->setFloat("minH", g_app->m_mesh.m_geom.m_minH);
	g_app->m_mainShader->setFloat("maxH", g_app->m_mesh.m_geom.m_maxH);
	glDrawElements(GL_TRIANGLES, g_app->m_mesh.m_geom.m_index.size(), GL_UNSIGNED_INT, 0);
	if (g_ui.shading_mode == SM_COLOR)
	{
		glLineWidth(1.5f);
		glUseProgram(g_app->m_wireframeShader.m_program);
		g_app->m_wireframeShader.setMat4f("model", g_app->m_mesh.m_model);
		g_app->m_wireframeShader.setMat4f("view", g_app->m_cam.m_view);
		g_app->m_wireframeShader.setMat4f("proj", g_app->m_cam.m_proj);
		glDrawElements(GL_TRIANGLES, g_app->m_mesh.m_geom.m_index.size(), GL_UNSIGNED_INT, 0);
		glLineWidth(2.5f);
	}

	if (g_ui.draw_T1)
	{
		glUseProgram(g_app->m_principalDirT1.m_program);
		g_app->m_principalDirT1.setMat4f("model", g_app->m_mesh.m_model);
		g_app->m_principalDirT1.setMat4f("view", g_app->m_cam.m_view);
		g_app->m_principalDirT1.setMat4f("proj", g_app->m_cam.m_proj);
		glDrawElements(GL_TRIANGLES, g_app->m_mesh.m_geom.m_index.size(), GL_UNSIGNED_INT, 0);
	}

	if (g_ui.draw_T2)
	{
		glUseProgram(g_app->m_principalDirT2.m_program);
		g_app->m_principalDirT2.setMat4f("model", g_app->m_mesh.m_model);
		g_app->m_principalDirT2.setMat4f("view", g_app->m_cam.m_view);
		g_app->m_principalDirT2.setMat4f("proj", g_app->m_cam.m_proj);
		glDrawElements(GL_TRIANGLES, g_app->m_mesh.m_geom.m_index.size(), GL_UNSIGNED_INT, 0);
	}

	glBindVertexArray(0);
}

void render()
{
	// ImGui new frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	draw_mesh();

	// draw UI
	draw_UI();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGui_init(GLFWwindow* win)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(win, true);
	ImGui_ImplOpenGL3_Init("#version 410");
}

int main(int argc, char* argv[])
{
	// init glfw
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Suggestive Contours", nullptr, nullptr);
	if (!window)
	{
		std::cout << "Failed to initialize the GLFW window." << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// init glad
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD." << std::endl;
		return -1;
	}

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glViewport(0, 0, WIDTH, HEIGHT);
	glLineWidth(2.5f);

	// IMGUI init
	ImGui_init(window);

	// ui init
	g_ui.draw_T1 = false;
	g_ui.draw_T2 = false;
	g_ui.object_color[0] = 1.0f;
	g_ui.object_color[1] = 1.0f;
	g_ui.object_color[2] = 1.0f;
	g_ui.shading_mode = SM_COLOR;
	g_ui.draw_true_contours = false;
	g_ui.draw_suggestive_contours = false;
	g_ui.max_Kn = 0.085f;

	// application render loop
	g_app = std::make_unique<struct App>();
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	while (!glfwWindowShouldClose(window))
	{
		processInput(window);
		if (g_app->m_mouse.m_changed)
		{
			g_app->m_cam.rotateView(g_app->m_mouse, g_app->m_viewport);
			g_app->m_mouse.m_changed = false;
		}

		render();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// clean ImGui
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
	return 0;
}