#include <SDL2/SDL_opengl.h>
#include "GL/glu.h"

static int component_count(GLenum format)
{
    switch (format) {
    case GL_RED: case GL_ALPHA: case GL_LUMINANCE: return 1;
    case GL_LUMINANCE_ALPHA: return 2;
    case GL_RGB: return 3;
    case GL_RGBA: return 4;
    default: return 0;
    }
}

GLint gluScaleImage(GLenum format, GLsizei widthin, GLsizei heightin, GLenum typein, const void *datain,
                    GLsizei widthout, GLsizei heightout, GLenum typeout, void *dataout)
{
    const unsigned char *source = (const unsigned char *)datain;
    unsigned char *destination = (unsigned char *)dataout;
    const int components = component_count(format);
    int x, y, component;
    if (!source || !destination || widthin <= 0 || heightin <= 0 || widthout <= 0 || heightout <= 0) return GLU_INVALID_VALUE;
    if (typein != GL_UNSIGNED_BYTE || typeout != GL_UNSIGNED_BYTE || !components) return GLU_INVALID_ENUM;
    for (y = 0; y < heightout; ++y) {
        const int source_y = (y * heightin) / heightout;
        for (x = 0; x < widthout; ++x) {
            const int source_x = (x * widthin) / widthout;
            const unsigned char *from = source + ((source_y * widthin + source_x) * components);
            unsigned char *to = destination + ((y * widthout + x) * components);
            for (component = 0; component < components; ++component) to[component] = from[component];
        }
    }
    return 0;
}
