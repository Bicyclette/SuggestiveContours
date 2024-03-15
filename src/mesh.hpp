#pragma once

#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <Eigen/unsupported/Eigen/MatrixFunctions>
#include <vector>
#include <array>
#include <glm/gtx/string_cast.hpp>
#include <random>
#include "camera.hpp"
#include "shader.hpp"

constexpr float g_halfPI = glm::pi<float>() / 2.0f;

float triangle_corner_angle(glm::vec3 const& corner, glm::vec3 const& a, glm::vec3 const& b);
float compute_voronoi_region_of_vertex_in_triangle(glm::vec3 const& vertex, glm::vec3 const& a, glm::vec3 const& b);
float cot(float angle);
float triangle_area(glm::vec3 const& a, glm::vec3 const& b, glm::vec3 const& c);
float gen_random(float from, float to);

struct CoordSys
{
	glm::vec3 m_u;
	glm::vec3 m_v;
	glm::vec3 m_w; // normal/up
};

struct MatCube
{
	// ========== constructors
	MatCube() : m_a(0.0f), m_b(0.0f){}
	MatCube(float a, float b, float c, float d)
	{
		m_a[0][0] = a; m_a[0][1] = b;
		m_a[1][0] = b; m_a[1][1] = c;

		m_b[0][0] = b; m_b[0][1] = c;
		m_b[1][0] = c; m_b[1][1] = d;
	}

	// ========== variables
	glm::mat2 m_a;
	glm::mat2 m_b;

	// ========== operators
	struct MatCube operator+=(struct MatCube const& iCube)
	{
		this->m_a += iCube.m_a;
		this->m_b += iCube.m_b;
		return *this;
	}
	struct MatCube operator/=(float const& iValue)
	{
		this->m_a /= iValue;
		this->m_b /= iValue;
		return *this;
	}
	friend glm::mat2 operator*(struct MatCube const& iCube, glm::vec2 const& iVec)
	{
		glm::vec2 col1 = iCube.m_a * iVec;
		glm::vec2 col2 = iCube.m_b * iVec;
		glm::mat2 res(col1, col2);
		return res;
	}
	friend struct MatCube operator*(struct MatCube const& iCube, float iValue)
	{
		struct MatCube res;
		res.m_a = iCube.m_a * iValue;
		res.m_b = iCube.m_b * iValue;
		return res;
	}
};

struct Geometry
{
	// vertices'data
	std::vector<glm::vec3> m_vertex;
	std::vector<glm::vec3> m_vertex_normal;
	std::vector<std::vector<int>> m_neighboring_faces;
	std::vector<std::vector<int>> m_neighboring_vertices;
	std::vector<glm::vec3> m_t1;							// principal direction t1
	std::vector<glm::vec3> m_t2;							// principal direction t2
	std::vector<float> m_K1;								// principal curvature K1 (max) computed from curvature tensor
	std::vector<float> m_K2;								// principal curvature K1 (min) computed from curvature tensor
	std::vector<struct CoordSys> m_vertex_coordSys;
	std::vector<glm::mat2> m_vertex_weingarten;				// weingarten matrix for each vertex (curvature tensor)
	std::vector<struct MatCube> m_vertex_C;

	// faces'data
	std::vector<glm::ivec3> m_face;
	std::vector<glm::vec3> m_face_normal;
	std::vector<unsigned int> m_index;
	std::vector<glm::mat2> m_face_weingarten;				// weingarten matrix for each face (curvature tensor)
	std::vector<glm::vec3> m_face_weingarten_weights;
	std::vector<struct MatCube> m_face_C;
	std::vector<struct CoordSys> m_face_coordSys;

	// Taubin smoothing
	float Kpb;
	float lambda;
	unsigned int N;
	float mu;
	Eigen::MatrixXd m_W; // weights matrix
	Eigen::MatrixXd m_K; // circulant matrix

	void compute_circulant_matrix();
	float phi(glm::vec3 vi, glm::vec3 vj);
	Eigen::MatrixXd transfer_function(Eigen::MatrixXd& m);

	// curvatures
	float m_minKg;
	float m_maxKg;
	float m_minH;
	float m_maxH;
	void compute_per_face_weingarten_matrix();
	void compute_face_mixed_voronoi_area(glm::vec3 const& a, glm::vec3 const& b, glm::vec3 const& c, glm::vec3 & weights);
	void compute_per_vertex_weingarten_matrix();
	void compute_min_max();
	void compute_per_face_C();
	void compute_per_vertex_C();
};

struct Mesh
{
	Mesh(std::string const & iPath);
	~Mesh();
	void create_GPU_objects();
	void get_neighboring_faces(int vertex_id, std::vector<int>& neighbors);
	void get_neighbors(int vertex_id, std::vector<int>& neighboring_vertices, std::vector<int>& neighboring_faces);
	void taubin_smoothing();
	void update_pos_vbo();
	void update_normal_vbo();
	void update_curvature_vbos();

	struct Geometry m_geom;
	GLuint m_vao;
	GLuint m_posvbo;
	GLuint m_normalvbo;
	GLuint m_tangent_U;
	GLuint m_tangent_V;
	GLuint m_K1Vbo;
	GLuint m_K2Vbo;
	GLuint m_T1Vbo;
	GLuint m_T2Vbo;
	GLuint m_C1Vbo;
	GLuint m_C2Vbo;
	GLuint m_ebo;
	glm::mat4 m_model;
};