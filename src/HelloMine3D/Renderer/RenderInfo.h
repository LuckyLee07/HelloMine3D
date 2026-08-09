#ifndef RENDERINFO_H_INCLUDED
#define RENDERINFO_H_INCLUDED

#include <glad/glad.h>

struct RenderInfo {
    GLuint vao = 0;
    GLuint indicesCount = 0;

    inline void reset()
    {
        vao = 0;
        indicesCount = 0;
    }
};

#endif // RENDERINFO_H_INCLUDED
