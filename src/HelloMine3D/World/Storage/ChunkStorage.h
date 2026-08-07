#ifndef CHUNKSTORAGE_H_INCLUDED
#define CHUNKSTORAGE_H_INCLUDED

#include <string>

class Chunk;

class ChunkStorage {
  public:
    ChunkStorage();
    explicit ChunkStorage(std::string rootDirectory);

    bool loadChunk(Chunk &chunk) const;
    bool saveChunk(const Chunk &chunk) const;

    std::string chunkPath(int x, int z) const;

  private:
    bool ensureRootDirectory() const;

    std::string m_rootDirectory;
};

#endif // CHUNKSTORAGE_H_INCLUDED
