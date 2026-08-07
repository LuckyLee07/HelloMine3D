#ifndef CHUNKMESH_H_INCLUDED
#define CHUNKMESH_H_INCLUDED

#include "../../Renderer/Model.h"

#include <SFML/Graphics.hpp>
#include <array>
#include <vector>

class ChunkMesh {
  public:
    ChunkMesh() = default;

    void addFace(const std::array<GLfloat, 12> &blockFace,
                 const std::array<GLfloat, 8> &textureCoords,
                 const sf::Vector3i &chunkPosition,
                 const sf::Vector3i &blockPosition, GLfloat cardinalLight);

    void bufferMesh();

    const Model &getModel() const;

    void clearClientData();
    void deleteData();

    /// Takes over CPU mesh data built off the world lock. The GPU model is
    /// left alone: it belongs to the section, not to the freshly built data.
    void adoptClientData(ChunkMesh &source);

    int faces = 0;

  private:
    Mesh m_mesh;
    Model m_model;
    std::vector<GLfloat> m_light;
    GLuint m_indexIndex = 0;
};

struct ChunkMeshCollection {
    ChunkMesh solidMesh;
    ChunkMesh waterMesh;
    ChunkMesh floraMesh;

    void adoptClientData(ChunkMeshCollection &source)
    {
        solidMesh.adoptClientData(source.solidMesh);
        waterMesh.adoptClientData(source.waterMesh);
        floraMesh.adoptClientData(source.floraMesh);
    }
};

#endif // CHUNKMESH_H_INCLUDED
