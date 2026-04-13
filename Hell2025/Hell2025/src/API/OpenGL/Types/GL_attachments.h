#pragma once
#include <glad/glad.h>
#include <string>

struct ColorAttachment {
    std::string name = "undefined";
    GLuint handle = 0;
    GLenum internalFormat = 0;
    GLenum format = 0;
    GLenum type = 0;
};

struct DepthAttachment {
    GLuint handle = 0;
    GLenum internalFormat = 0;
};