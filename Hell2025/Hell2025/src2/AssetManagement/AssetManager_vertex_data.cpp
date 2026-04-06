#include "AssetManager.h"

namespace AssetManager {

    std::span<Vertex> GetVerticesSpan(uint32_t baseVertex, uint32_t vertexCount) {
        std::vector<Vertex>& vertices = GetVertices();
        return std::span<Vertex>(vertices.data() + baseVertex, vertexCount);
    }

    std::span<uint32_t> GetIndicesSpan(uint32_t baseIndex, uint32_t indexCount) {
        std::vector<uint32_t>& indices = GetIndices();
        return std::span<uint32_t>(indices.data() + baseIndex, indexCount);
    }
}