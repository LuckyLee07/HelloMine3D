#ifndef TEXTUREATLAS_H_INCLUDED
#define TEXTUREATLAS_H_INCLUDED

#include "BasicTexture.h"

/// @brief Texture atlas that pulls texture data from existing files and maps them appropraitely.
class TextureAtlas : public BasicTexture {
  public:
    TextureAtlas(const std::string &textureFileName);
};

#endif // TEXTUREATLAS_H_INCLUDED
