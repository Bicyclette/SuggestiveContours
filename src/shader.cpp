#include "shader.hpp"

std::string file2String(std::string const & iFile)
{
	std::ifstream fileStream(iFile.c_str());
	if (fileStream.is_open())
	{
		std::stringstream buffer;
		buffer << fileStream.rdbuf();
		return buffer.str();
	}
	else
	{
		std::cerr << "Error: failed opening file \"" << iFile << "\"" << std::endl;
		std::exit(-1);
	}
}

void Shader::checkCompileError(GLuint const & iShader, GLenum iType)
{
	int success;
	int logLength;
	std::unique_ptr<char> log;

	glGetShaderiv(iShader, GL_COMPILE_STATUS, &success);
	if (success == GL_FALSE)
	{
		glGetShaderiv(iShader, GL_INFO_LOG_LENGTH, &logLength);
		log = std::make_unique<char>(logLength);
		glGetShaderInfoLog(iShader, logLength, nullptr, log.get());
		std::cerr << "Error while compiling the " << ((iType == GL_VERTEX_SHADER) ? "vertex" : "fragment") << " shader : " << log << std::endl;
		glDeleteShader(iShader);
	}
}

bool Shader::checkLinkError()
{
	int success;
	int logLength;
	std::unique_ptr<char> log;

	glGetProgramiv(m_program, GL_LINK_STATUS, &success);
	if (success == GL_FALSE)
	{
		glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &logLength);
		log = std::make_unique<char>(logLength);
		glGetProgramInfoLog(m_program, logLength, nullptr, log.get());
		std::cerr << "Error while linking shaders into a program : " << log << std::endl;
		return false;
	}
	return true;
}

Shader::Shader(std::string const& iVertex, std::string const& iFragment)
{
	GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
	std::string vShaderString = file2String(iVertex);
	const GLchar* vShaderSource = (const GLchar*)vShaderString.c_str();
	glShaderSource(vShader, 1, &vShaderSource, NULL);
	glCompileShader(vShader);

	GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
	std::string fShaderString = file2String(iFragment);
	const GLchar* fShaderSource = (const GLchar*)fShaderString.c_str();
	glShaderSource(fShader, 1, &fShaderSource, NULL);
	glCompileShader(fShader);

	// Check for errors
	checkCompileError(vShader, GL_VERTEX_SHADER);
	checkCompileError(fShader, GL_FRAGMENT_SHADER);

	// create program
	m_program = glCreateProgram();
	glAttachShader(m_program, vShader);
	glAttachShader(m_program, fShader);
	glLinkProgram(m_program);
	if (!checkLinkError())
	{
		glDeleteShader(vShader);
		glDeleteShader(fShader);
	}
	glDetachShader(m_program, vShader);
	glDetachShader(m_program, fShader);
}

Shader::Shader(std::string const & iVertex, std::string const& iGeometry, std::string const& iFragment)
{
	GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
	std::string vShaderString = file2String(iVertex);
	const GLchar* vShaderSource = (const GLchar*)vShaderString.c_str();
	glShaderSource(vShader, 1, &vShaderSource, NULL);
	glCompileShader(vShader);

	GLuint gShader = glCreateShader(GL_GEOMETRY_SHADER);
	std::string gShaderString = file2String(iGeometry);
	const GLchar* gShaderSource = (const GLchar*)gShaderString.c_str();
	glShaderSource(gShader, 1, &gShaderSource, NULL);
	glCompileShader(gShader);

	GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
	std::string fShaderString = file2String(iFragment);
	const GLchar* fShaderSource = (const GLchar*)fShaderString.c_str();
	glShaderSource(fShader, 1, &fShaderSource, NULL);
	glCompileShader(fShader);
	
	// Check for errors
	checkCompileError(vShader, GL_VERTEX_SHADER);
	checkCompileError(fShader, GL_GEOMETRY_SHADER);
	checkCompileError(fShader, GL_FRAGMENT_SHADER);

	// create program
	m_program = glCreateProgram();
	glAttachShader(m_program, vShader);
	glAttachShader(m_program, gShader);
	glAttachShader(m_program, fShader);
	glLinkProgram(m_program);
	if (!checkLinkError())
	{
		glDeleteShader(vShader);
		glDeleteShader(gShader);
		glDeleteShader(fShader);
	}
	glDetachShader(m_program, vShader);
	glDetachShader(m_program, gShader);
	glDetachShader(m_program, fShader);
}

Shader::~Shader()
{
	glDeleteProgram(m_program);
}

void Shader::setMat4f(std::string const & iUniform, glm::mat4 const& iMat)
{
	glUniformMatrix4fv(glGetUniformLocation(m_program, iUniform.c_str()), 1, GL_FALSE, glm::value_ptr(iMat));
}

void Shader::setVec3f(std::string const& iUniform, glm::vec3 const& iVec)
{
	glUniform3f(glGetUniformLocation(m_program, iUniform.c_str()), iVec.x, iVec.y, iVec.z);
}

void Shader::setInt(std::string const& iUniform, int const& iValue)
{
	glUniform1i(glGetUniformLocation(m_program, iUniform.c_str()), iValue);
}

void Shader::setFloat(std::string const& iUniform, float const& iValue)
{
	glUniform1f(glGetUniformLocation(m_program, iUniform.c_str()), iValue);
}

void Shader::setBool(std::string const& iUniform, bool const& iValue)
{
	glUniform1i(glGetUniformLocation(m_program, iUniform.c_str()), iValue);
}