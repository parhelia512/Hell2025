#pragma once
#include "API/OpenGL/Types/GL_mesh_buffer.h"

#define GRASS_SIZE 40

struct GrassMesh {

    void Create();

    glm::vec3 m_bladePoints[GRASS_SIZE][GRASS_SIZE];

    OpenGLMeshBuffer glMesh;
};