#ifndef DOOM64EX_UWP_GLU_H
#define DOOM64EX_UWP_GLU_H

#include <SDL2/SDL_opengl.h>

#define GLU_INVALID_ENUM 100900
#define GLU_INVALID_VALUE 100901

#ifdef __cplusplus
extern "C" {
#endif
GLint gluScaleImage(GLenum format, GLsizei widthin, GLsizei heightin, GLenum typein, const void *datain,
                    GLsizei widthout, GLsizei heightout, GLenum typeout, void *dataout);
#ifdef __cplusplus
}
#endif
#endif
