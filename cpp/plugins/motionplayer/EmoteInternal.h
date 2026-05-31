#pragma once

#if defined(_MSC_VER) || defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <GLES3/gl32.h>
#endif

namespace emoteplayer {

extern GLuint emotenodeprogram;
extern GLuint emotenodeVAO;
extern GLfloat default_control_points[32];

bool emoteUseLegacyGL();
void evaluatePatchPosition(float u, float v, int surfaceCount, const float* transforms,
                           const float* const* controlPoints, float& outX, float& outY,
                           float& outZ, float& outW);
void emoteDrawPatch(int surfaceCount, const float* transforms,
                    const float* const* controlPoints);
void emoteRestoreGLState();

GLuint createEmptyTexture(int width, int height);
GLuint createEmptyDepthTexture(int width, int height);
GLuint createFBO(GLuint texture, GLuint depthtexture);
void glBaseSet();
void glBaseSetWithoutClear();

} // namespace emoteplayer
