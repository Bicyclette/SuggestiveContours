#include "mesh.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

// Create a mesh from an OBJ file
Mesh::Mesh(std::string const & iPath)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::string warn;
	std::string err;

	bool triangulate = true;
	tinyobj::LoadObj(&attrib, &shapes, nullptr, &warn, &err, iPath.c_str(), nullptr, triangulate);
	if (!warn.empty()) { std::cerr << "WARN: " << warn << std::endl; }
	if (!err.empty()) { std::cerr << err << std::endl; }

	for (size_t v = 0; v < attrib.vertices.size(); v += 3)
	{
		m_geom.m_vertex.emplace_back(attrib.vertices[v], attrib.vertices[v + 1], attrib.vertices[v + 2]);
	}

	std::vector<tinyobj::index_t> const& indices = shapes[0].mesh.indices;
	m_geom.m_vertex_normal.resize(m_geom.m_vertex.size());
	m_geom.m_neighboring_faces.resize(m_geom.m_vertex.size());
	m_geom.m_neighboring_vertices.resize(m_geom.m_vertex.size());
	m_geom.m_face_normal.resize(indices.size() / 3);

	for (size_t i = 0; i < indices.size(); i += 3)
	{
		int v0 = indices[i].vertex_index;
		int v1 = indices[i+1].vertex_index;
		int v2 = indices[i+2].vertex_index;
		m_geom.m_face.emplace_back(v0, v1, v2);
		m_geom.m_index.push_back(v0);
		m_geom.m_index.push_back(v1);
		m_geom.m_index.push_back(v2);

		// compute face normal
		glm::vec3 e0 = m_geom.m_vertex[v1] - m_geom.m_vertex[v0];
		glm::vec3 e1 = m_geom.m_vertex[v2] - m_geom.m_vertex[v0];
		glm::vec3 normal = glm::normalize(glm::cross(e0, e1));
		m_geom.m_face_normal[i/3] = normal;
	}

	// compute vertex normal
	for (size_t i = 0; i < m_geom.m_vertex.size(); ++i)
	{
		std::vector<int> neighboring_faces;
		std::vector<int> neighboring_vertices;
		get_neighbors(i, neighboring_vertices, neighboring_faces);

		glm::vec3 vertex_normal(0.0f, 0.0f, 0.0f);
		for (int const& face_idx : neighboring_faces)
		{
			glm::vec3 face_normal = m_geom.m_face_normal[face_idx];
			vertex_normal += face_normal;
		}
		vertex_normal = glm::normalize(vertex_normal);

		m_geom.m_vertex_normal[i] = vertex_normal;
		m_geom.m_neighboring_faces[i] = neighboring_faces;
		m_geom.m_neighboring_vertices[i] = neighboring_vertices;
	}

	// compute per face weingarten's matrix
	m_geom.compute_per_face_weingarten_matrix();

	// compute per vertex weingarten's matrix
	m_geom.m_t1.resize(m_geom.m_vertex.size());
	m_geom.m_t2.resize(m_geom.m_vertex.size());
	m_geom.m_K1.resize(m_geom.m_vertex.size());
	m_geom.m_K2.resize(m_geom.m_vertex.size());
	m_geom.compute_per_vertex_weingarten_matrix();

	// compute min & max (Kg & H)
	m_geom.m_minKg = 0.0f;
	m_geom.m_maxKg = 0.0f;
	m_geom.m_minH = 0.0f;
	m_geom.m_maxH = 0.0f;
	m_geom.compute_min_max();

	// compute C matrix
	m_geom.m_face_C.resize(m_geom.m_face.size());
	m_geom.m_vertex_C.resize(m_geom.m_vertex.size());
	m_geom.compute_per_face_C();
	m_geom.compute_per_vertex_C();

	// send geometry data to GPU
	create_GPU_objects();

	// model matrix
	m_model = glm::mat4(1.0f);

	// geometry's circulant matrix
	m_geom.compute_circulant_matrix();

	// low-pass transfer function
	m_geom.Kpb = 0.095f;
	m_geom.lambda = 0.6307f;
	m_geom.N = 25;
	m_geom.mu = m_geom.lambda / ((m_geom.lambda * m_geom.Kpb) - 1.0f); // from 1/lambda + 1/mu = Kpb
}

Mesh::~Mesh()
{
	glBindVertexArray(m_vao);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &m_posvbo);
	glDeleteBuffers(1, &m_normalvbo);
	glDeleteBuffers(1, &m_tangent_U);
	glDeleteBuffers(1, &m_tangent_V);
	glDeleteBuffers(1, &m_K1Vbo);
	glDeleteBuffers(1, &m_K2Vbo);
	glDeleteBuffers(1, &m_T1Vbo);
	glDeleteBuffers(1, &m_T2Vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &m_ebo);
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &m_vao);
}

void Mesh::create_GPU_objects()
{
	// VAO
	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);

	// POSITION VBO
	glGenBuffers(1, &m_posvbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_posvbo);
	glBufferData(GL_ARRAY_BUFFER, m_geom.m_vertex.size() * sizeof(glm::vec3), m_geom.m_vertex.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	glEnableVertexAttribArray(0);

	// NORMAL VBO
	glGenBuffers(1, &m_normalvbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_normalvbo);
	glBufferData(GL_ARRAY_BUFFER, m_geom.m_vertex_normal.size() * sizeof(glm::vec3), m_geom.m_vertex_normal.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	glEnableVertexAttribArray(1);


	// tangent plane to each vertex
	std::vector<glm::vec3> tangentU;
	std::vector<glm::vec3> tangentV;
	for (size_t i = 0; i < m_geom.m_vertex_coordSys.size(); ++i)
	{
		glm::vec3 u = m_geom.m_vertex_coordSys[i].m_u;
		glm::vec3 v = m_geom.m_vertex_coordSys[i].m_v;
		tangentU.push_back(u);
		tangentV.push_back(v);
	}

	// TANGENT PLANE U
	glGenBuffers(1, &m_tangent_U);
	glBindBuffer(GL_ARRAY_BUFFER, m_tangent_U);
	glBufferData(GL_ARRAY_BUFFER, tangentU.size() * sizeof(glm::vec3), tangentU.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	glEnableVertexAttribArray(2);

	// TANGENT PLANE V
	glGenBuffers(1, &m_tangent_V);
	glBindBuffer(GL_ARRAY_BUFFER, m_tangent_V);
	glBufferData(GL_ARRAY_BUFFER, tangentV.size() * sizeof(glm::vec3), tangentV.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	glEnableVertexAttribArray(3);

	// K1 PRINCIPAL CURVATURE VBO
	glGenBuffers(1, &m_K1Vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_K1Vbo);
	glBufferData(GL_ARRAY_BUFFER, m_geom.m_K1.size() * sizeof(float), m_geom.m_K1.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
	glEnableVertexAttribArray(4);

	// K2 PRINCIPAL CURVATURE VBO
	glGenBuffers(1, &m_K2Vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_K2Vbo);
	glBufferData(GL_ARRAY_BUFFER, m_geom.m_K2.size() * sizeof(float), m_geom.m_K2.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
	glEnableVertexAttribArray(5);

	// tensor T1 PRINCIPAL DIRECTION VBO
	glGenBuffers(1, &m_T1Vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_T1Vbo);
	glBufferData(GL_ARRAY_BUFFER, m_geom.m_t1.size() * sizeof(glm::vec3), m_geom.m_t1.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	glEnableVertexAttribArray(6);

	// tensor T2 PRINCIPAL DIRECTION VBO
	glGenBuffers(1, &m_T2Vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_T2Vbo);
	glBufferData(GL_ARRAY_BUFFER, m_geom.m_t2.size() * sizeof(glm::vec3), m_geom.m_t2.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	glEnableVertexAttribArray(7);

	// tensor C1 VBO
	std::vector<glm::mat2> C1;
	for (auto const& m : m_geom.m_vertex_C) { C1.push_back(m.m_a); }
	glGenBuffers(1, &m_C1Vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_C1Vbo);
	glBufferData(GL_ARRAY_BUFFER, C1.size() * sizeof(glm::mat2), C1.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat2), (void*)0);
	glEnableVertexAttribArray(8);

	// tensor C2 VBO
	std::vector<glm::mat2> C2;
	for (auto const& m : m_geom.m_vertex_C) { C2.push_back(m.m_b); }
	glGenBuffers(1, &m_C2Vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_C2Vbo);
	glBufferData(GL_ARRAY_BUFFER, C2.size() * sizeof(glm::mat2), C2.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat2), (void*)0);
	glEnableVertexAttribArray(9);

	// ELEMENT EBO
	glGenBuffers(1, &m_ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_geom.m_index.size() * sizeof(unsigned int), m_geom.m_index.data(), GL_STATIC_DRAW);

	// Unbind VAO
	glBindVertexArray(0);
}

void Mesh::get_neighboring_faces(int vertex_id, std::vector<int>& neighbors)
{
	for (int i = 0; i < m_geom.m_face.size(); ++i)
	{
		int idv0 = m_geom.m_face[i].x;
		int idv1 = m_geom.m_face[i].y;
		int idv2 = m_geom.m_face[i].z;

		if (vertex_id == idv0 || vertex_id == idv1 || vertex_id == idv2)
		{
			neighbors.push_back(i);
		}
	}
}

void Mesh::get_neighbors(int vertex_id, std::vector<int>& neighboring_vertices, std::vector<int>& neighboring_faces)
{
	get_neighboring_faces(vertex_id, neighboring_faces);
	for (int& f : neighboring_faces)
	{
		int idv0 = m_geom.m_face[f].x;
		int idv1 = m_geom.m_face[f].y;
		int idv2 = m_geom.m_face[f].z;

		if (vertex_id == idv0)
		{
			if (std::find(neighboring_vertices.begin(), neighboring_vertices.end(), idv1) == neighboring_vertices.end()) {
				neighboring_vertices.push_back(idv1);
			}
			if (std::find(neighboring_vertices.begin(), neighboring_vertices.end(), idv2) == neighboring_vertices.end()) {
				neighboring_vertices.push_back(idv2);
			}
		}
		else if (vertex_id == idv1)
		{
			if (std::find(neighboring_vertices.begin(), neighboring_vertices.end(), idv0) == neighboring_vertices.end()) {
				neighboring_vertices.push_back(idv0);
			}
			if (std::find(neighboring_vertices.begin(), neighboring_vertices.end(), idv2) == neighboring_vertices.end()) {
				neighboring_vertices.push_back(idv2);
			}
		}
		else if (vertex_id == idv2)
		{
			if (std::find(neighboring_vertices.begin(), neighboring_vertices.end(), idv0) == neighboring_vertices.end()) {
				neighboring_vertices.push_back(idv0);
			}
			if (std::find(neighboring_vertices.begin(), neighboring_vertices.end(), idv1) == neighboring_vertices.end()) {
				neighboring_vertices.push_back(idv1);
			}
		}
	}
}

void Mesh::taubin_smoothing()
{
	Eigen::MatrixXd Kn = m_geom.transfer_function(m_geom.m_K);
	Eigen::MatrixXd x = Eigen::MatrixXd::Zero(m_geom.m_vertex.size(), 3);
	for (size_t i = 0; i < m_geom.m_vertex.size(); ++i)
	{
		glm::vec3 v = m_geom.m_vertex[i];
		x(i, 0) = v.x;
		x(i, 1) = v.y;
		x(i, 2) = v.z;
	}
	Eigen::MatrixXd x_prime = Kn * x;
	for (size_t i = 0; i < m_geom.m_vertex.size(); ++i)
	{
		m_geom.m_vertex[i].x = x_prime(i, 0);
		m_geom.m_vertex[i].y = x_prime(i, 1);
		m_geom.m_vertex[i].z = x_prime(i, 2);
	}

	for (size_t i = 0; i < m_geom.m_index.size(); i += 3)
	{
		int v0 = m_geom.m_index[i];
		int v1 = m_geom.m_index[i + 1];
		int v2 = m_geom.m_index[i + 2];

		// compute face normal
		glm::vec3 e0 = m_geom.m_vertex[v1] - m_geom.m_vertex[v0];
		glm::vec3 e1 = m_geom.m_vertex[v2] - m_geom.m_vertex[v0];
		glm::vec3 normal = glm::normalize(glm::cross(e0, e1));
		m_geom.m_face_normal[i / 3] = normal;
	}

	// compute vertex normal
	for (size_t i = 0; i < m_geom.m_vertex.size(); ++i)
	{
		std::vector<int> neighboring_faces;
		std::vector<int> neighboring_vertices;
		get_neighbors(i, neighboring_vertices, neighboring_faces);

		glm::vec3 vertex_normal(0.0f, 0.0f, 0.0f);
		for (int const& face_idx : m_geom.m_neighboring_faces[i])
		{
			glm::vec3 face_normal = m_geom.m_face_normal[face_idx];
			vertex_normal += face_normal;
		}
		vertex_normal = glm::normalize(vertex_normal);

		m_geom.m_vertex_normal[i] = vertex_normal;
	}

	// compute per face weingarten's matrix
	m_geom.compute_per_face_weingarten_matrix();

	// compute per vertex weingarten's matrix
	m_geom.compute_per_vertex_weingarten_matrix();

	// compute min & max (Kg & H)
	m_geom.m_minKg = 0.0f;
	m_geom.m_maxKg = 0.0f;
	m_geom.m_minH = 0.0f;
	m_geom.m_maxH = 0.0f;
	m_geom.compute_min_max();

	// compute C matrix
	m_geom.compute_per_face_C();
	m_geom.compute_per_vertex_C();

	glBindVertexArray(m_vao);
	update_pos_vbo();
	update_normal_vbo();
	update_curvature_vbos();
	glBindVertexArray(0);
}

void Geometry::compute_circulant_matrix()
{
	size_t dimension = m_vertex.size();
	m_W = Eigen::MatrixXd::Zero(dimension, dimension);
	m_K = Eigen::MatrixXd::Identity(dimension, dimension);

	for (size_t i = 0; i < dimension; ++i)
	{
		glm::vec3 const& vi = m_vertex[i];
		std::vector<int> const& neighbors_of_i = m_neighboring_vertices[i];
		
		float sum_phi_vi_vj = 0.0f;
		for (int const& j : neighbors_of_i)
		{
			glm::vec3 const& vj = m_vertex[j];
			sum_phi_vi_vj += phi(vi, vj);
		}
		for (int const& j : neighbors_of_i)
		{
			glm::vec3 const& vj = m_vertex[j];
			m_W(i, j) = phi(vi, vj) / sum_phi_vi_vj;
			m_K(i, j) -= m_W(i, j);
		}
	}
}

float Geometry::phi(glm::vec3 vi, glm::vec3 vj)
{
	const float alpha = -1.0f;
	glm::vec3 edge = vi - vj;
	float norm = sqrt(edge.x * edge.x + edge.y * edge.y + edge.z * edge.z);
	return pow(norm, alpha);
}

Eigen::MatrixXd Geometry::transfer_function(Eigen::MatrixXd& m)
{
	Eigen::MatrixXd I = Eigen::MatrixXd::Identity(m_vertex.size(), m_vertex.size());
	Eigen::MatrixXd res = (I - (lambda * m)) * (I - (mu * m));
	Eigen::MatrixXd resPow(res.pow(N));
	return resPow;
}

void Mesh::update_pos_vbo()
{
	glBindBuffer(GL_ARRAY_BUFFER, m_posvbo);
	void* data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	float* vertex_pos = reinterpret_cast<float*>(data);
	for (size_t i = 0; i < m_geom.m_vertex.size(); ++i)
	{
		glm::vec3 v = m_geom.m_vertex[i];
		vertex_pos[3 * i] = v.x;
		vertex_pos[3 * i + 1] = v.y;
		vertex_pos[3 * i + 2] = v.z;
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);
}

void Mesh::update_normal_vbo()
{
	glBindBuffer(GL_ARRAY_BUFFER, m_normalvbo);
	void* data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	float* vertex_normal = reinterpret_cast<float*>(data);
	for (size_t i = 0; i < m_geom.m_vertex_normal.size(); ++i)
	{
		glm::vec3 n = m_geom.m_vertex_normal[i];
		vertex_normal[3 * i] = n.x;
		vertex_normal[3 * i + 1] = n.y;
		vertex_normal[3 * i + 2] = n.z;
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);
}

void Mesh::update_curvature_vbos()
{
	glBindBuffer(GL_ARRAY_BUFFER, m_K1Vbo);
	void* data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	float* K1 = reinterpret_cast<float*>(data);
	for (size_t i = 0; i < m_geom.m_K1.size(); ++i)
	{
		float k = m_geom.m_K1[i];
		K1[i] = k;
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);

	glBindBuffer(GL_ARRAY_BUFFER, m_K2Vbo);
	data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	float* K2 = reinterpret_cast<float*>(data);
	for (size_t i = 0; i < m_geom.m_K2.size(); ++i)
	{
		float k = m_geom.m_K2[i];
		K2[i] = k;
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);

	glBindBuffer(GL_ARRAY_BUFFER, m_T1Vbo);
	data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	float* T1 = reinterpret_cast<float*>(data);
	for (size_t i = 0; i < m_geom.m_t1.size(); ++i)
	{
		glm::vec3 t = m_geom.m_t1[i];
		T1[i * 3] = t.x;
		T1[i * 3 + 1] = t.y;
		T1[i * 3 + 2] = t.z;
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);

	glBindBuffer(GL_ARRAY_BUFFER, m_T2Vbo);
	data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	float* T2 = reinterpret_cast<float*>(data);
	for (size_t i = 0; i < m_geom.m_t2.size(); ++i)
	{
		glm::vec3 t = m_geom.m_t2[i];
		T2[i * 3] = t.x;
		T2[i * 3 + 1] = t.y;
		T2[i * 3 + 2] = t.z;
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);

	std::vector<glm::mat2> C1;
	for (auto const& m : m_geom.m_vertex_C) { C1.push_back(m.m_a); }
	glBindBuffer(GL_ARRAY_BUFFER, m_C1Vbo);
	data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	float* c1 = reinterpret_cast<float*>(data);
	for (size_t i = 0; i < C1.size(); ++i)
	{
		glm::mat2 m = C1[i];
		const float* m_array = reinterpret_cast<const float*>(glm::value_ptr(m));
		c1[i * 4] = m_array[0];
		c1[i * 4 + 1] = m_array[1];
		c1[i * 4 + 2] = m_array[2];
		c1[i * 4 + 3] = m_array[3];
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);

	std::vector<glm::mat2> C2;
	for (auto const& m : m_geom.m_vertex_C) { C2.push_back(m.m_b); }
	glBindBuffer(GL_ARRAY_BUFFER, m_C2Vbo);
	data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	float* c2 = reinterpret_cast<float*>(data);
	for (size_t i = 0; i < C2.size(); ++i)
	{
		glm::mat2 m = C2[i];
		const float* m_array = reinterpret_cast<const float*>(glm::value_ptr(m));
		c2[i * 4] = m_array[0];
		c2[i * 4 + 1] = m_array[1];
		c2[i * 4 + 2] = m_array[2];
		c2[i * 4 + 3] = m_array[3];
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);
}

void Geometry::compute_per_face_weingarten_matrix()
{
	m_face_coordSys.clear();
	m_face_weingarten.clear();
	m_face_weingarten_weights.clear();

	for (size_t i = 0; i < m_face.size(); ++i)
	{
		int idv0 = m_face[i].x;
		int idv1 = m_face[i].y;
		int idv2 = m_face[i].z;

		// get face vertices and per vertex normals
		glm::vec3 v0 = m_vertex[idv0];
		glm::vec3 n0 = m_vertex_normal[idv0];
		glm::vec3 v1 = m_vertex[idv1];
		glm::vec3 n1 = m_vertex_normal[idv1];
		glm::vec3 v2 = m_vertex[idv2];
		glm::vec3 n2 = m_vertex_normal[idv2];

		// compute edges
		glm::vec3 e0 = v1 - v0;
		glm::vec3 e1 = v2 - v1;
		glm::vec3 e2 = v0 - v2;

		// compute face's coordinate system
		struct CoordSys cs;
		cs.m_u = glm::normalize(e0);
		cs.m_v = glm::normalize(glm::cross(cs.m_u, glm::cross(e1, -e0)));
		cs.m_w = glm::normalize(glm::cross(e1, -e0));
		m_face_coordSys.push_back(cs);

		// solve curvature tensor matrix by using linear least squares
		Eigen::MatrixXf A = Eigen::MatrixXf::Zero(6, 4);
		A(0, 0) = glm::dot(e0, cs.m_u); A(0, 1) = glm::dot(e0, cs.m_v);
		A(1, 2) = glm::dot(e0, cs.m_u); A(1, 3) = glm::dot(e0, cs.m_v);
		A(2, 0) = glm::dot(e1, cs.m_u); A(2, 1) = glm::dot(e1, cs.m_v);
		A(3, 2) = glm::dot(e1, cs.m_u); A(3, 3) = glm::dot(e1, cs.m_v);
		A(4, 0) = glm::dot(e2, cs.m_u); A(4, 1) = glm::dot(e2, cs.m_v);
		A(5, 2) = glm::dot(e2, cs.m_u); A(5, 3) = glm::dot(e2, cs.m_v);

		Eigen::MatrixXf b = Eigen::MatrixXf::Zero(6, 1);
		b(0, 0) = glm::dot((n1 - n0), cs.m_u);
		b(1, 0) = glm::dot((n1 - n0), cs.m_v);
		b(2, 0) = glm::dot((n2 - n1), cs.m_u);
		b(3, 0) = glm::dot((n2 - n1), cs.m_v);
		b(4, 0) = glm::dot((n0 - n2), cs.m_u);
		b(5, 0) = glm::dot((n0 - n2), cs.m_v);

		Eigen::Vector4f x = A.colPivHouseholderQr().solve(b);

		glm::mat2 m;
		m[0][0] = x(0); m[0][1] = x(1);
		m[1][0] = x(2); m[1][1] = x(3);
		m_face_weingarten.push_back(m);
		
		// compute the curvature tensor weights for each of its vertices
		glm::vec3 weights;
		compute_face_mixed_voronoi_area(v0, v1, v2, weights);
		m_face_weingarten_weights.push_back(weights);
	}
}

void Geometry::compute_face_mixed_voronoi_area(glm::vec3 const& a, glm::vec3 const& b, glm::vec3 const& c, glm::vec3& weights)
{
	// for the triangle with corner a
	float angle_a = triangle_corner_angle(a, b, c);
	// for the triangle with corner b
	float angle_b = triangle_corner_angle(b, a, c);
	// for the triangle with corner c
	float angle_c = triangle_corner_angle(c, a, b);
 
	// check if non obtuse
	if (angle_a < g_halfPI && angle_b < g_halfPI && angle_c < g_halfPI)
	{
		weights[0] = compute_voronoi_region_of_vertex_in_triangle(a, b, c);
		weights[1] = compute_voronoi_region_of_vertex_in_triangle(b, c, a);
		weights[2] = compute_voronoi_region_of_vertex_in_triangle(c, a, b);
	}
	else
	{
		float half_area = triangle_area(a, b, c) / 2.0f;
		float quarter_area = triangle_area(a, b, c) / 4.0f;

		if (angle_a >= g_halfPI) {
			weights[0] = half_area;
		}
		else {
			weights[0] = quarter_area;
		}

		if (angle_b >= g_halfPI) {
			weights[1] = half_area;
		}
		else {
			weights[1] = quarter_area;
		}

		if (angle_c >= g_halfPI) {
			weights[2] = half_area;
		}
		else {
			weights[2] = quarter_area;
		}

	}
}

void Geometry::compute_per_vertex_weingarten_matrix()
{
	m_vertex_coordSys.clear();
	m_vertex_weingarten.clear();
	glm::mat2 init{ 0.0f, 0.0f, 0.0f, 0.0f };
	m_vertex_weingarten.assign(m_vertex.size(), init);

	for (size_t i = 0; i < m_vertex.size(); ++i)
	{
		glm::vec3 const& x = m_vertex[i];
		glm::vec3 const& n = m_vertex_normal[i];

		// build vertex coordinate system
		float d = glm::dot(n, x);
		float randX = gen_random(0.05f, 0.95f);
		float randY = gen_random(0.05f, 0.95f);
		glm::vec3 u(randX, randY, 0.0f);
		u.z = (-(n.x * u.x + n.y * u.y) + d) / n.z;
		u = glm::normalize(u);
		glm::vec3 v = glm::normalize(glm::cross(n, u));

		struct CoordSys vertex_cs;
		vertex_cs.m_u = u;
		vertex_cs.m_v = v;
		vertex_cs.m_w = n;
		m_vertex_coordSys.push_back(vertex_cs);

		// express curvature tensor of all surrounding faces
		// in terms of the current vertex coordinate system
		std::vector<int> const& neighboring_faces = m_neighboring_faces[i];
		float sum_weights = 0.0f;

		for (int const& fIdx : neighboring_faces)
		{
			glm::ivec3 face = m_face[fIdx];
			struct CoordSys const& face_coordSys = m_face_coordSys[fIdx];
			glm::vec3 const& face_normal = face_coordSys.m_w;

			// get face voronoi area weight associated to current vertex
			glm::vec3 weights = m_face_weingarten_weights[fIdx];
			float weight = 0.0f;
			if (face.x == i) { weight = weights.x; }
			else if (face.y == i) { weight = weights.y; }
			else { weight = weights.z; }
			sum_weights += weight;

			// get curvature tensor of face, and vector quantities of the vertex and face's coordinate systems
			glm::mat2 const& curvatureTensor = m_face_weingarten[fIdx];
			glm::vec3 Up = vertex_cs.m_u;
			glm::vec3 Vp = vertex_cs.m_v;
			glm::vec3 Uf = face_coordSys.m_u;
			glm::vec3 Vf = face_coordSys.m_v;

			// check if normals are parallel (compute rotation or not ?)
			float cos_normals = glm::dot(face_normal, n);
			if (cos_normals > 0.998f) // parallel => no rotation
			{
				// now get curvature tensor
				// in this new coordinate system (Uf, Vf) : vertex coordinate system expressed in face's one
				float UpUf = glm::dot(Up, Uf);
				float UpVf = glm::dot(Up, Vf);
				float VpUf = glm::dot(Vp, Uf);
				float VpVf = glm::dot(Vp, Vf);

				float ep = glm::dot(glm::normalize(glm::vec2(UpUf, UpVf)), (curvatureTensor * glm::normalize(glm::vec2(UpUf, UpVf))));
				float fp = glm::dot(glm::normalize(glm::vec2(UpUf, UpVf)), (curvatureTensor * glm::normalize(glm::vec2(VpUf, VpVf))));
				float gp = glm::dot(glm::normalize(glm::vec2(VpUf, VpVf)), (curvatureTensor * glm::normalize(glm::vec2(VpUf, VpVf))));

				glm::mat2 tensor;
				tensor[0][0] = ep;
				tensor[0][1] = fp;
				tensor[1][0] = fp;
				tensor[1][1] = gp;

				m_vertex_weingarten[i] += weight * tensor;
			}
			else
			{
				// axis of rotation for coordinate system transform
				glm::vec3 axis = glm::cross(face_normal, n);
				axis = glm::normalize(axis);

				// compute rotation angle from face coordinate system
				// to the current vertex coordinate system
				float angle = acos( glm::dot(face_normal, n) / (glm::length(face_normal) * glm::length(n)) );
				glm::quat q = glm::angleAxis(angle, axis);
				
				// rotate face coordinate system
				glm::vec3 face_up = q * face_coordSys.m_u;
				glm::vec3 face_vp = q * face_coordSys.m_v;
				glm::vec3 face_wp = q * face_coordSys.m_w;

				// now get curvature tensor
				// in this new coordinate system (Uf, Vf) : vertex coordinate system expressed in face's one
				Uf = face_up;
				Vf = face_vp;
				
				float UpUf = glm::dot(Up, Uf);
				float UpVf = glm::dot(Up, Vf);
				float VpUf = glm::dot(Vp, Uf);
				float VpVf = glm::dot(Vp, Vf);

				float ep = glm::dot(glm::normalize(glm::vec2(UpUf, UpVf)), (curvatureTensor * glm::normalize(glm::vec2(UpUf, UpVf))));
				float fp = glm::dot(glm::normalize(glm::vec2(UpUf, UpVf)), (curvatureTensor * glm::normalize(glm::vec2(VpUf, VpVf))));
				float gp = glm::dot(glm::normalize(glm::vec2(VpUf, VpVf)), (curvatureTensor * glm::normalize(glm::vec2(VpUf, VpVf))));

				glm::mat2 tensor;
				tensor[0][0] = ep;
				tensor[0][1] = fp;
				tensor[1][0] = fp;
				tensor[1][1] = gp;

				m_vertex_weingarten[i] += weight * tensor;
			}
		}
		m_vertex_weingarten[i] /= sum_weights;

		// compute eigen values and eigen vectors of the curvature tensor
		Eigen::Matrix2f eigenMat;
		eigenMat(0, 0) = m_vertex_weingarten[i][0][0];
		eigenMat(0, 1) = m_vertex_weingarten[i][0][1];
		eigenMat(1, 0) = m_vertex_weingarten[i][1][0];
		eigenMat(1, 1) = m_vertex_weingarten[i][1][1];

		Eigen::EigenSolver<Eigen::MatrixXf> solver;
		solver.compute(eigenMat, true);
		Eigen::Vector2f eigenValues = solver.eigenvalues().real();
		Eigen::Matrix2f eigenVectors = solver.eigenvectors().real();

		m_K1[i] = eigenValues(1);
		m_K2[i] = eigenValues(0);
		m_t1[i] = glm::normalize(vertex_cs.m_u * eigenVectors.col(1)(0) + vertex_cs.m_v * eigenVectors.col(1)(1));
		m_t2[i] = glm::normalize(vertex_cs.m_u * eigenVectors.col(0)(0) + vertex_cs.m_v * eigenVectors.col(0)(1));
	}
}

float triangle_corner_angle(glm::vec3 const& corner, glm::vec3 const& a, glm::vec3 const& b)
{
	glm::vec3 e1 = a - corner;
	float e1_length = glm::length(e1);
	glm::vec3 e2 = b - corner;
	float e2_length = glm::length(e2);
	float theta = acos(glm::dot(e1, e2) / (e1_length * e2_length));
	return theta;
}

float compute_voronoi_region_of_vertex_in_triangle(glm::vec3 const& vertex, glm::vec3 const& a, glm::vec3 const& b)
{
	glm::vec3 side1 = vertex - a;
	float length_side1 = glm::length(side1);
	glm::vec3 side2 = vertex - b;
	float length_side2 = glm::length(side2);
	
	float angle_a = triangle_corner_angle(a, vertex, b);
	float angle_b = triangle_corner_angle(b, vertex, a);

	return (pow(length_side1, 2.0f) * cot(angle_b) + pow(length_side2, 2.0f) * cot(angle_a)) / 8.0f;
}

float cot(float angle)
{
	return cos(angle) / sin(angle);
}

float triangle_area(glm::vec3 const& a, glm::vec3 const& b, glm::vec3 const& c)
{
	glm::vec3 e1 = b - a;
	float e1_length = glm::length(e1);
	glm::vec3 e2 = c - a;
	float e2_length = glm::length(e2);
	float theta = acos(glm::dot(e1, e2) / (e1_length * e2_length));
	return (e1_length * e2_length * sin(theta)) / 2.0;
}

float gen_random(float from, float to)
{
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_real_distribution<float> dist(from, to);
	return dist(mt);
}

void Geometry::compute_min_max()
{
	// compute min and max Gaussian curvature
	m_minKg = m_K1[0] * m_K2[0];
	m_maxKg = m_K1[0] * m_K2[0];
	for (size_t i = 1; i < m_vertex.size(); ++i)
	{
		float Kg = m_K1[i] * m_K2[i];
		if (Kg < m_minKg) { m_minKg = Kg; }
		if (Kg > m_maxKg) { m_maxKg = Kg; }
	}

	// compute min and max Mean curvature
	m_minH = (m_K1[0] + m_K2[0]) / 2.0f;
	m_maxH = (m_K1[0] + m_K2[0]) / 2.0f;
	for (size_t i = 1; i < m_vertex.size(); ++i)
	{
		float H = (m_K1[i] + m_K2[i]) / 2.0f;
		if (H < m_minH) { m_minH = H; }
		if (H > m_maxH) { m_maxH = H; }
	}
}

void Geometry::compute_per_face_C()
{
	size_t dimension = m_face.size();
	for (size_t i = 0; i < dimension; ++i)
	{
		int idv0 = m_face[i].x;
		int idv1 = m_face[i].y;
		int idv2 = m_face[i].z;

		// get edges
		glm::vec3 e0 = m_vertex[idv1] - m_vertex[idv0];
		glm::vec3 e1 = m_vertex[idv2] - m_vertex[idv1];
		glm::vec3 e2 = m_vertex[idv0] - m_vertex[idv2];

		// get all second fundamental form matrices
		glm::mat2 sff_v0 = m_vertex_weingarten[idv0];
		glm::mat2 sff_v1 = m_vertex_weingarten[idv1];
		glm::mat2 sff_v2 = m_vertex_weingarten[idv2];

		// get face coordinate system
		struct CoordSys cs = m_face_coordSys[i];

		// solve face's C matrix by using linear least squares
		Eigen::MatrixXf A = Eigen::MatrixXf::Zero(9, 4);
		A(0, 0) = glm::dot(e0, cs.m_u); A(0, 1) = glm::dot(e0, cs.m_v);
		A(1, 1) = glm::dot(e0, cs.m_u); A(1, 2) = glm::dot(e0, cs.m_v);
		A(2, 2) = glm::dot(e0, cs.m_u); A(2, 3) = glm::dot(e0, cs.m_v);
		A(3, 0) = glm::dot(e1, cs.m_u); A(3, 1) = glm::dot(e1, cs.m_v);
		A(4, 1) = glm::dot(e1, cs.m_u); A(4, 2) = glm::dot(e1, cs.m_v);
		A(5, 2) = glm::dot(e1, cs.m_u); A(5, 3) = glm::dot(e1, cs.m_v);
		A(6, 0) = glm::dot(e2, cs.m_u); A(6, 1) = glm::dot(e2, cs.m_v);
		A(7, 1) = glm::dot(e2, cs.m_u); A(7, 2) = glm::dot(e2, cs.m_v);
		A(8, 2) = glm::dot(e2, cs.m_u); A(8, 3) = glm::dot(e2, cs.m_v);

		Eigen::MatrixXf b = Eigen::MatrixXf::Zero(9, 1);
		b(0, 0) = ((sff_v1 - sff_v0) * glm::vec2(cs.m_u)).x;
		b(1, 0) = ((sff_v1 - sff_v0) * glm::vec2(cs.m_u)).y;
		b(2, 0) = ((sff_v1 - sff_v0) * glm::vec2(cs.m_v)).y;
		b(3, 0) = ((sff_v2 - sff_v1) * glm::vec2(cs.m_u)).x;
		b(4, 0) = ((sff_v2 - sff_v1) * glm::vec2(cs.m_u)).y;
		b(5, 0) = ((sff_v2 - sff_v1) * glm::vec2(cs.m_v)).y;
		b(6, 0) = ((sff_v0 - sff_v2) * glm::vec2(cs.m_u)).x;
		b(7, 0) = ((sff_v0 - sff_v2) * glm::vec2(cs.m_u)).y;
		b(8, 0) = ((sff_v0 - sff_v2) * glm::vec2(cs.m_v)).y;

		Eigen::Vector4f x = A.colPivHouseholderQr().solve(b);

		struct MatCube m(x(0), x(1), x(2), x(3));
		m_face_C[i] = m;
	}
}

void Geometry::compute_per_vertex_C()
{
	for (size_t i = 0; i < m_vertex.size(); ++i)
	{
		glm::vec3 const& vertex_normal = m_vertex_normal[i];

		// express C matrices of all surrounding faces
		// in terms of the current vertex coordinate system
		std::vector<int> const& neighboring_faces = m_neighboring_faces[i];
		float sum_weights = 0.0f;

		for (int const& fIdx : neighboring_faces)
		{
			glm::ivec3 face = m_face[fIdx];
			struct CoordSys const& face_coordSys = m_face_coordSys[fIdx];
			glm::vec3 const& face_normal = face_coordSys.m_w;

			// get face voronoi area weight associated to current vertex
			glm::vec3 weights = m_face_weingarten_weights[fIdx];
			float weight = 0.0f;
			if (face.x == i) { weight = weights.x; }
			else if (face.y == i) { weight = weights.y; }
			else { weight = weights.z; }
			sum_weights += weight;

			// get face's C matrix, and vector quantities of the vertex and face's coordinate systems
			struct MatCube const& C = m_face_C[fIdx];
			glm::vec3 Up = m_vertex_coordSys[i].m_u;
			glm::vec3 Vp = m_vertex_coordSys[i].m_v;
			glm::vec3 Uf = face_coordSys.m_u;
			glm::vec3 Vf = face_coordSys.m_v;

			// check if normals are parallel (compute rotation or not ?)
			float cos_normals = glm::dot(face_normal, vertex_normal);
			if (cos_normals > 0.998f) // parallel => no rotation
			{
				// now get vertex C matrix
				// in this new coordinate system (Uf, Vf) : vertex coordinate system expressed in face's one
				float UpUf = glm::dot(Up, Uf);
				float UpVf = glm::dot(Up, Vf);
				float VpUf = glm::dot(Vp, Uf);
				float VpVf = glm::dot(Vp, Vf);

				float a = glm::dot(glm::normalize(glm::vec2(UpUf, UpVf)), ((C * glm::normalize(glm::vec2(UpUf, UpVf))) * glm::normalize(glm::vec2(UpUf, UpVf))));
				float b = glm::dot(glm::normalize(glm::vec2(UpUf, UpVf)), ((C * glm::normalize(glm::vec2(VpUf, VpVf))) * glm::normalize(glm::vec2(UpUf, UpVf))));
				float c = glm::dot(glm::normalize(glm::vec2(VpUf, VpVf)), ((C * glm::normalize(glm::vec2(VpUf, VpVf))) * glm::normalize(glm::vec2(UpUf, UpVf))));
				float d = glm::dot(glm::normalize(glm::vec2(VpUf, VpVf)), ((C * glm::normalize(glm::vec2(VpUf, VpVf))) * glm::normalize(glm::vec2(VpUf, VpVf))));

				struct MatCube tensorC(a, b, c, d);
				m_vertex_C[i] += tensorC * weight;
			}
			else
			{
				// axis of rotation for coordinate system transform
				glm::vec3 axis = glm::cross(face_normal, vertex_normal);
				axis = glm::normalize(axis);

				// compute rotation angle from face coordinate system
				// to the current vertex coordinate system
				float angle = acos(glm::dot(face_normal, vertex_normal) / (glm::length(face_normal) * glm::length(vertex_normal)));
				glm::quat q = glm::angleAxis(angle, axis);

				// rotate face coordinate system
				glm::vec3 face_up = q * face_coordSys.m_u;
				glm::vec3 face_vp = q * face_coordSys.m_v;
				glm::vec3 face_wp = q * face_coordSys.m_w;

				// now get curvature tensor
				// in this new coordinate system (Up, Vp) : vertex coordinate system
				Uf = face_up;
				Vf = face_vp;

				float UpUf = glm::dot(Up, Uf);
				float UpVf = glm::dot(Up, Vf);
				float VpUf = glm::dot(Vp, Uf);
				float VpVf = glm::dot(Vp, Vf);

				float a = glm::dot(glm::normalize(glm::vec2(UpUf, UpVf)), ((C * glm::normalize(glm::vec2(UpUf, UpVf))) * glm::normalize(glm::vec2(UpUf, UpVf))));
				float b = glm::dot(glm::normalize(glm::vec2(UpUf, UpVf)), ((C * glm::normalize(glm::vec2(VpUf, VpVf))) * glm::normalize(glm::vec2(UpUf, UpVf))));
				float c = glm::dot(glm::normalize(glm::vec2(VpUf, VpVf)), ((C * glm::normalize(glm::vec2(VpUf, VpVf))) * glm::normalize(glm::vec2(UpUf, UpVf))));
				float d = glm::dot(glm::normalize(glm::vec2(VpUf, VpVf)), ((C * glm::normalize(glm::vec2(VpUf, VpVf))) * glm::normalize(glm::vec2(VpUf, VpVf))));

				struct MatCube tensorC(a, b, c, d);
				m_vertex_C[i] += tensorC * weight;
			}
		}
		if (sum_weights != 0.0f)
		{
			m_vertex_C[i] /= sum_weights;
		}
	}
}