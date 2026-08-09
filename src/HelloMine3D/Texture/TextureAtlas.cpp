#include "TextureAtlas.h"

#include "../Util/ResourcePaths.h"

TextureAtlas::TextureAtlas(const std::string& textureFileName)
{
    sf::Image i;
    if (!i.loadFromFile(ResourcePaths::media("textures/" + textureFileName + ".png")))
    {
        throw std::runtime_error("Unable to open image: " + textureFileName);
    }
    loadFromImage(i);
}
