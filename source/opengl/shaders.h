#ifndef SHADERS_H
#define SHADERS_H

#include <iostream>
#include "defaultUniforms.h"

namespace emp {
  namespace opengl {

    class Uniform {
      private:
      GLint handle;

      public:
      Uniform(GLint handle) : handle(handle) {}
      operator GLint() const { return handle; }

      template <typename T>
      void set(T&& value) {
        setUniform(handle, std::forward<T>(value));
      }
    };

    enum class ShaderType : GLenum {
      Vertex = GL_VERTEX_SHADER,
      Fragment = GL_FRAGMENT_SHADER,
    };

    std::ostream& operator<<(std::ostream& out, const ShaderType& st) {
      if (st == ShaderType::Vertex) {
        return out << "GL_VERTEX_SHADER";
      } else {
        return out << "GL_FRAGMENT_SHADER";
      }
    }

    class Shader {
      private:
      GLuint handle = 0;

      public:
      Shader(GLuint handle) : handle(handle) {}
      Shader(const char* source, ShaderType type)
        : handle(glCreateShader(static_cast<GLenum>(type))) {
        glShaderSource(handle, 1, &source, nullptr);
        glCompileShader(handle);

        GLint success;
        glGetShaderiv(handle, GL_COMPILE_STATUS, &success);
        if (success == GL_FALSE) {
          GLsizei maxLogLength = 0;
          glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &maxLogLength);
          maxLogLength += 1;
          auto log = new GLchar[maxLogLength];

          GLsizei logLength;
          glGetShaderInfoLog(handle, maxLogLength, &logLength, log);

          std::cout << "ERROR IN " << type << ":" << std::endl
                    << std::string(log, logLength) << std::endl;
        }
      }

      Shader(const Shader&) = delete;
      Shader(Shader&& other) : handle(other.handle) { other.handle = 0; }

      Shader& operator=(const Shader&) = delete;
      Shader& operator=(Shader&& other) {
        if (this != &other) {
          destroy();
          std::swap(handle, other.handle);
        }

        return *this;
      }

      ~Shader() { destroy(); }

      operator GLuint() const { return handle; }
      operator bool() const { return handle != 0; }

      void destroy() {
        if (handle != 0) {
          glDeleteShader(handle);
          handle = 0;
        }
      }
    };

    class VertexShader : public Shader {
      public:
      VertexShader(GLuint handle) : Shader(handle) {}
      VertexShader(const char* source) : Shader(source, ShaderType::Vertex) {}
      VertexShader(const VertexShader&) = delete;
      VertexShader(VertexShader&& other) : Shader(std::move(other)) {}

      VertexShader& operator=(VertexShader&& other) {
        Shader::operator=(std::move(other));
        return *this;
      }

      VertexShader& operator=(const VertexShader&) = delete;
    };

    class FragmentShader : public Shader {
      public:
      FragmentShader(GLuint handle) : Shader(handle) {}
      FragmentShader(const char* source)
        : Shader(source, ShaderType::Fragment) {}
      FragmentShader(const FragmentShader&) = delete;
      FragmentShader(FragmentShader&& other) : Shader(std::move(other)) {}

      FragmentShader& operator=(FragmentShader&& other) {
        Shader::operator=(std::move(other));
        return *this;
      }

      FragmentShader& operator=(const FragmentShader&) = delete;
    };

    class ShaderProgram {
      private:
      GLuint handle;

      public:
      ShaderProgram(const VertexShader& vertexShader,
                    const FragmentShader& fragmentShader)
        : ShaderProgram(glCreateProgram()) {
        glAttachShader(handle, vertexShader);
        glAttachShader(handle, fragmentShader);
        glLinkProgram(handle);

        GLint linkStatus;
        glGetProgramiv(handle, GL_LINK_STATUS, &linkStatus);
        if (linkStatus == GL_FALSE) {
          GLsizei maxLogLength = 0;
          glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &maxLogLength);
          maxLogLength += 1;
          auto log = new GLchar[maxLogLength];

          GLsizei logLength;
          glGetProgramInfoLog(handle, maxLogLength, &logLength, log);

          std::cout << std::string(log, logLength) << std::endl;
        }
      }

      explicit ShaderProgram(GLuint handle) : handle(handle) {}

      ShaderProgram(const ShaderProgram&) = delete;
      ShaderProgram(ShaderProgram&& other) : handle(other.handle) {
        other.handle = 0;
      }

      ShaderProgram& operator=(const ShaderProgram&) = delete;
      ShaderProgram& operator=(ShaderProgram&& other) {
        if (this != &other) {
          destory();
          std::swap(handle, other.handle);
        }
        return *this;
      }

      ~ShaderProgram() {}

      void destory() {
        if (handle != 0) {
          glDeleteProgram(handle);
          handle = 0;
        }
      }

      void use() {
        if (handle != 0) {
          glUseProgram(handle);
        }
      }

      operator GLuint() const { return handle; }
      operator bool() const { return handle != 0; }

      VertexAttribute getAttribute(const std::string& name,
                                   VertexAttributeSize size,
                                   VertexAttributeType type, GLsizei stride = 0,
                                   const void* offset = nullptr) {
        auto loc = glGetAttribLocation(handle, name.c_str());

        return VertexAttribute(loc, size, type, stride, offset);
      }

      FloatingVertexAttribute getAttribute(const std::string& name,
                                           VertexAttributeSize size,
                                           FloatingVertexAttributeType type,
                                           GLsizei stride = 0,
                                           const void* offset = nullptr,
                                           bool normalized = false) {
        auto loc = glGetAttribLocation(handle, name.c_str());

        return FloatingVertexAttribute(loc, size, type, normalized, stride,
                                       offset);
      }

      template <typename T, typename... Args>
      decltype(auto) attribute(const std::string& name, Args&&... args) {
        using attribs = VertexAttributes<T>;
        return getAttribute(name, attribs::size, attribs::type,
                            std::forward<Args>(args)...);
      }

      template <typename U, typename T, typename... Args>
      decltype(auto) attribute(const std::string& name, T U::*member,
                               Args&&... args) {
        using attribs = VertexAttributes<T>;

        auto offset =
          reinterpret_cast<std::uintptr_t>(&(static_cast<U*>(0)->*member)) -
          reinterpret_cast<std::uintptr_t>(static_cast<U*>(0));

        return getAttribute(name, attribs::size, attribs::type, sizeof(U),
                            reinterpret_cast<const void*>(offset),
                            std::forward<Args>(args)...);
      }

      Uniform uniform(const std::string& name) const {
        return glGetUniformLocation(handle, name.c_str());
      }
    };
  }  // namespace opengl
}  // namespace emp

#endif