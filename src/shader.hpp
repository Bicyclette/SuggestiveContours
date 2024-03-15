#pragma once

#include <sstream>
#include <fstream>
#include <string>
#include <memory>
#include "GLCommon.h"

std::string file2String(std::string const & iFile);

struct Shader
{
	Shader() = delete;
	Shader(std::string const& iVertex, std::string const& iFragment);
	Shader(std::string const& iVertex, std::string const& iGeometry, std::string const& iFragment);
	~Shader();
	void checkCompileError(GLuint const& iShader, GLenum iType);
	bool checkLinkError();
	void setMat4f(std::string const& iUniform, glm::mat4 const & iMat);
	void setVec3f(std::string const& iUniform, glm::vec3 const& iVec);
	void setInt(std::string const& iUniform, int const& iValue);
	void setFloat(std::string const& iUniform, float const& iValue);
	void setBool(std::string const& iUniform, bool const& iValue);

	GLuint m_program;
};