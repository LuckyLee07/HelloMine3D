#pragma once

#include <OgreManualObject.h>

// GL3Plus reuses a VAO for each program. Omitting an active uv3 from a manual
// mesh can retain the preceding terrain draw's root buffer, so every ordinary
// vertex must bind the explicit zero owner alongside its existing attributes.
inline void appendOrdinaryManualVertexAttributes(
    Ogre::ManualObject &object, float tileU = 0.f, float tileV = 0.f,
    float repeatU = 0.f, float repeatV = 0.f, float light = 1.f)
{
    object.textureCoord(tileU, tileV);
    object.textureCoord(repeatU, repeatV);
    object.textureCoord(light);
    object.textureCoord(0.f);
}
