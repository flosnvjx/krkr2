#include "EmoteInternal.h"
#include "EmoteGLBridge.h"

#include <SDL2/SDL_log.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace emoteplayer {
#pragma region glprogram

GLuint emotenodeprogram = 0;
GLuint emotenodeVAO = 0;
extern GLfloat default_control_points[32];
bool emoteUseLegacyGL();

namespace {

bool g_glProfileDetected = false;
bool g_useLegacyGL = false;

static const int kLegacyTessDiv = 16;

struct LegacyVertex
{
    float x, y, z, w;
    float tu, tv;
};

GLuint g_legacyVBO = 0;
GLuint g_legacyIBO = 0;
bool g_legacyIndicesReady = false;
GLuint g_patchVBO = 0;
bool g_patchVerticesReady = false;

void ensurePatchVertices()
{
    if (g_patchVerticesReady || emoteUseLegacyGL() || emotenodeVAO == 0)
        return;

    if (g_patchVBO == 0)
        glGenBuffers(1, &g_patchVBO);

    glBindVertexArray(emotenodeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_patchVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(default_control_points), default_control_points,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    g_patchVerticesReady = true;
}

bool detectLegacyGL()
{
    if (!TVPEnsureEmoteGLContext())
        return true;
    const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    int major = 0;
    if (ver != nullptr)
        std::sscanf(ver, "%d", &major);
    const bool legacy = major < 4;
    SDL_Log("Emote GL profile: %s (legacy=%s)", ver != nullptr ? ver : "unknown",
            legacy ? "yes" : "no");
    return legacy;
}

void ensureLegacyIndices()
{
    if (g_legacyIndicesReady)
        return;
    if (g_legacyIBO == 0)
        glGenBuffers(1, &g_legacyIBO);
    std::vector<GLushort> indices;
    indices.reserve(static_cast<size_t>(kLegacyTessDiv) * kLegacyTessDiv * 6);
    const int stride = kLegacyTessDiv + 1;
    for (int iy = 0; iy < kLegacyTessDiv; ++iy)
    {
        for (int ix = 0; ix < kLegacyTessDiv; ++ix)
        {
            const GLushort i0 = static_cast<GLushort>(iy * stride + ix);
            const GLushort i1 = static_cast<GLushort>(i0 + 1);
            const GLushort i2 = static_cast<GLushort>(i0 + stride);
            const GLushort i3 = static_cast<GLushort>(i2 + 1);
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i3);
            indices.push_back(i2);
        }
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_legacyIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(GLushort)), indices.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    g_legacyIndicesReady = true;
}

static const char* legacyVertexShaderSrc = R"(#version 120
attribute vec4 aPos;
attribute vec2 aTexCoord;
varying vec2 tessCoord;
void main()
{
    gl_Position = aPos;
    tessCoord = aTexCoord;
}
)";

static const char* legacyFragmentShaderSrc = R"(#version 120
varying vec2 tessCoord;
uniform sampler2D texture1;
uniform bool enableMask;
uniform vec2 viewportSize;
uniform sampler2D maskTexture;
uniform float opa;
void main()
{
    vec4 maskColor = vec4(1.0, 1.0, 1.0, 1.0);
    if (enableMask) {
        vec2 normalizedCoord = gl_FragCoord.xy / viewportSize;
        maskColor = texture2D(maskTexture, normalizedCoord);
    }
    vec4 color = texture2D(texture1, tessCoord);
    if (enableMask && maskColor.a < 0.5) {
        discard;
    }
    color.a = color.a * opa;
    gl_FragColor = color;
}
)";

} // namespace

namespace {

float cubicBlend(float p0, float p1, float p2, float p3, float t)
{
    const float mt = 1.0f - t;
    return mt * mt * mt * p0 + 3.0f * mt * mt * t * p1 + 3.0f * mt * t * t * p2 +
           t * t * t * p3;
}

void bezierSurface2D(float u, float v, const float* controlPts, float& outX, float& outY)
{
    float curveX[4];
    float curveY[4];
    for (int row = 0; row < 4; ++row)
    {
        const float* base = controlPts + row * 8;
        curveX[row] = cubicBlend(base[0], base[2], base[4], base[6], u);
        curveY[row] = cubicBlend(base[1], base[3], base[5], base[7], u);
    }
    outX = cubicBlend(curveX[0], curveX[1], curveX[2], curveX[3], v);
    outY = cubicBlend(curveY[0], curveY[1], curveY[2], curveY[3], v);
}

} // namespace

void evaluatePatchPosition(float u, float v, int surfaceCount, const float* transforms,
                             const float* const* controlPoints, float& outX, float& outY,
                             float& outZ, float& outW)
{
    float px = u;
    float py = v;
    float pz = 0.0f;
    float pw = 0.0f;
    for (int i = 0; i < surfaceCount; ++i)
    {
        if (i > 0)
        {
            const float oldX = px;
            const float oldY = py;
            px = 1.0f - (oldY / 2.0f + 0.5f);
            py = 0.5f + oldX / 2.0f;
            pz = 0.0f;
            pw = 0.0f;
        }
        const float* cps = (controlPoints != nullptr && controlPoints[i] != nullptr)
                               ? controlPoints[i]
                               : default_control_points;
        float sx = 0.0f;
        float sy = 0.0f;
        bezierSurface2D(px, py, cps, sx, sy);
        const float* m = transforms + i * 16;
        const float tx = m[0] * sx + m[4] * sy + m[8] * 0.0f + m[12] * 1.0f;
        const float ty = m[1] * sx + m[5] * sy + m[9] * 0.0f + m[13] * 1.0f;
        const float tz = m[2] * sx + m[6] * sy + m[10] * 0.0f + m[14] * 1.0f;
        const float tw = m[3] * sx + m[7] * sy + m[11] * 0.0f + m[15] * 1.0f;
        px = tx;
        py = ty;
        pz = tz;
        pw = tw;
    }
    outX = px;
    outY = -py;
    outZ = pz;
    outW = pw;
}

bool emoteUseLegacyGL()
{
    if (!g_glProfileDetected)
    {
        g_useLegacyGL = detectLegacyGL();
        g_glProfileDetected = true;
    }
    return g_useLegacyGL;
}

void emoteDrawPatch(int surfaceCount, const float* transforms,
                    const float* const* controlPoints)
{
    if (surfaceCount <= 0 || transforms == nullptr)
        return;

    const int grid = kLegacyTessDiv + 1;
    std::vector<LegacyVertex> vertices(static_cast<size_t>(grid) * grid);
    for (int iy = 0; iy < grid; ++iy)
    {
        const float v = static_cast<float>(iy) / static_cast<float>(kLegacyTessDiv);
        for (int ix = 0; ix < grid; ++ix)
        {
            const float u = static_cast<float>(ix) / static_cast<float>(kLegacyTessDiv);
            float px = 0.0f;
            float py = 0.0f;
            float pz = 0.0f;
            float pw = 0.0f;
            evaluatePatchPosition(u, v, surfaceCount, transforms, controlPoints, px, py, pz, pw);
            LegacyVertex& vtx = vertices[static_cast<size_t>(iy) * grid + ix];
            vtx.x = px;
            vtx.y = py;
            vtx.z = pz;
            vtx.w = pw;
            vtx.tu = v;
            vtx.tv = u;
        }
    }

    if (g_legacyVBO == 0)
        glGenBuffers(1, &g_legacyVBO);
    ensureLegacyIndices();

    glBindBuffer(GL_ARRAY_BUFFER, g_legacyVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(LegacyVertex)),
                 vertices.data(), GL_STREAM_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(LegacyVertex),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(LegacyVertex),
                          reinterpret_cast<void*>(offsetof(LegacyVertex, tu)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_legacyIBO);
    glDrawElements(GL_TRIANGLES, kLegacyTessDiv * kLegacyTessDiv * 6, GL_UNSIGNED_SHORT, nullptr);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void emoteRestoreGLState()
{
    while (glGetError() != GL_NO_ERROR)
    {
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!emoteUseLegacyGL())
        glBlendEquation(GL_FUNC_ADD);
}

#if !defined(__ANDROID__)
#if defined(__APPLE__)
#define EMOTE_GLSL_VERSION "#version 410 core\n"
#define EMOTE_MAX_SURFACES 20
#else
#define EMOTE_GLSL_VERSION "#version 430 core\n"
#define EMOTE_MAX_SURFACES 64
#endif
static const char* vertexShaderSrc = EMOTE_GLSL_VERSION R"(
        layout (location = 0) in vec2 aPos;

        void main()
        {
            gl_Position = vec4(aPos.xy, 0.0, 1.0);
        }
        )";
static const char* tessControlShaderSrc = EMOTE_GLSL_VERSION R"(
        layout (vertices = 16) out;

        void main()
        {
            gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

            if (gl_InvocationID == 0)
            {
                gl_TessLevelInner[0] = 8.0;
                gl_TessLevelInner[1] = 8.0;
                gl_TessLevelOuter[0] = 16.0;
                gl_TessLevelOuter[1] = 16.0;
                gl_TessLevelOuter[2] = 16.0;
                gl_TessLevelOuter[3] = 16.0;
            }
        }
        )";
static const char* tessEvaluationShaderSrc = EMOTE_GLSL_VERSION R"(
        layout(quads, equal_spacing, ccw) in;

        out vec2 tessCoord;

        // 最大渲染深度 EMOTE_MAX_SURFACES
        uniform int surfaceCount;
        uniform mat4 transforms[EMOTE_MAX_SURFACES];
        uniform vec2 controlPoints[EMOTE_MAX_SURFACES][16];

        float B0(float t) { return (1.0 - t) * (1.0 - t) * (1.0 - t); }
        float B1(float t) { return 3.0 * t * (1.0 - t) * (1.0 - t); }
        float B2(float t) { return 3.0 * t * t * (1.0 - t); }
        float B3(float t) { return t * t * t; }
        vec2 bezierSurface(vec2 uv, int idx) {
            float u = uv.x;
            float v = uv.y;

            vec2 result = vec2(0.0);

            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    int index = i * 4 + j;

                    // 计算基函数乘积
                    float bu = 0.0;
                    float bv = 0.0;

                    if (i == 0) bu = B0(u);
                    else if (i == 1) bu = B1(u);
                    else if (i == 2) bu = B2(u);
                    else if (i == 3) bu = B3(u);

                    if (j == 0) bv = B0(v);
                    else if (j == 1) bv = B1(v);
                    else if (j == 2) bv = B2(v);
                    else if (j == 3) bv = B3(v);

                    float basis = bu * bv;
                    vec2 controlPoint = controlPoints[idx][index];
                    result += controlPoint * basis;
                }
            }

            return result;
        }

        void main(void)
        {
            float u = gl_TessCoord.x;
            float v = gl_TessCoord.y;

            vec4 lastPt = vec4(u, v, 0.0, 0.0);
            for (int i = 0; i < surfaceCount; i++) {
                if (i > 0) {
                    lastPt = vec4(1.0 - (lastPt.y / 2.0 + 0.5), 0.5 + lastPt.x / 2.0, 0.0, 0.0);
                }
                vec2 position = bezierSurface(lastPt.xy, i);
                lastPt = transforms[i] * vec4(position.xy, 0.0, 1.0);
            }
            gl_Position = lastPt * vec4(1.0, -1.0, 1.0, 1.0);

            tessCoord = vec2(gl_TessCoord.y, gl_TessCoord.x);
        }
        )";
#if defined(__APPLE__)
static const char* fragmentShaderSrc = EMOTE_GLSL_VERSION R"(
        out vec4 FragColor;
        in vec2 tessCoord;
        uniform sampler2D texture1;
        uniform bool enableMask;
        uniform vec2 viewportSize;
        uniform sampler2D maskTexture;
        uniform float opa;
        void main()
        {
            vec4 maskColor = vec4(1.0, 1.0, 1.0, 1.0);
            if (enableMask) {
                vec2 normalizedCoord = gl_FragCoord.xy / viewportSize;
                maskColor = texture(maskTexture, normalizedCoord);
            }

            vec4 color = texture(texture1, tessCoord);
            if (enableMask && maskColor.a < 0.5) {
                discard;
            } else {
                color.a = color.a * opa;
                FragColor = vec4(color.rgba);
            }
        }
    )";
#else
static const char* fragmentShaderSrc = EMOTE_GLSL_VERSION R"(
        out vec4 FragColor;
        in vec2 tessCoord;
        uniform sampler2D texture1;
        uniform bool enableMask = false;
        uniform vec2 viewportSize;
        uniform sampler2D maskTexture;
        uniform float opa = 1.0;
        void main()
        {
            vec4 maskColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
            if (enableMask) {
                vec2 normalizedCoord = gl_FragCoord.xy / viewportSize;
                maskColor = texture(maskTexture, normalizedCoord);
            }

            vec4 color = texture(texture1, tessCoord);
            if (enableMask && maskColor.a < 0.5) {
                discard;
            } else {
                color.a = color.a * opa;
                FragColor = vec4(color.rgba);
            }
        }
    )";
#endif
#undef EMOTE_GLSL_VERSION
#undef EMOTE_MAX_SURFACES
#else
static const char* vertexShaderSrc = R"(#version 320 es
        layout (location = 0) in vec2 aPos;

        void main()
        {
            gl_Position = vec4(aPos.xy, 0.0, 1.0);
        }
        )";
static const char* tessControlShaderSrc = R"(#version 320 es
        layout (vertices = 16) out;

        void main()
        {
            gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

            if (gl_InvocationID == 0)
            {
                gl_TessLevelInner[0] = 8.0;
                gl_TessLevelInner[1] = 8.0;
                gl_TessLevelOuter[0] = 16.0;
                gl_TessLevelOuter[1] = 16.0;
                gl_TessLevelOuter[2] = 16.0;
                gl_TessLevelOuter[3] = 16.0;
            }
        }
        )";
static const char* tessEvaluationShaderSrc = R"(#version 320 es
        layout(quads, equal_spacing, ccw) in;

        out vec2 tessCoord;

        // 最大渲染深度20，应该是够用了?
        uniform int surfaceCount;
        uniform mat4 transforms[20];
        uniform vec2 controlPoints[20][16];

        float B0(float t) { return (1.0 - t) * (1.0 - t) * (1.0 - t); }
        float B1(float t) { return 3.0 * t * (1.0 - t) * (1.0 - t); }
        float B2(float t) { return 3.0 * t * t * (1.0 - t); }
        float B3(float t) { return t * t * t; }
        vec2 bezierSurface(vec2 uv, int idx) {
            float u = uv.x;
            float v = uv.y;

            vec2 result = vec2(0.0);

            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    int index = i * 4 + j;

                    // 计算基函数乘积
                    float bu = 0.0;
                    float bv = 0.0;

                    if (i == 0) bu = B0(u);
                    else if (i == 1) bu = B1(u);
                    else if (i == 2) bu = B2(u);
                    else if (i == 3) bu = B3(u);

                    if (j == 0) bv = B0(v);
                    else if (j == 1) bv = B1(v);
                    else if (j == 2) bv = B2(v);
                    else if (j == 3) bv = B3(v);

                    float basis = bu * bv;
                    vec2 controlPoint = controlPoints[idx][index];
                    result += controlPoint * basis;
                }
            }

            return result;
        }

        void main(void)
        {
            float u = gl_TessCoord.x;
            float v = gl_TessCoord.y;

            vec4 lastPt = vec4(u, v, 0, 0);
            for (int i = 0; i < surfaceCount; i++) {
                if (i > 0) {
                    lastPt = vec4(1.0 - (lastPt.y / 2.0 + 0.5), 0.5 + lastPt.x / 2.0, 0.0, 0.0);
                }
                vec2 position = bezierSurface(lastPt.xy, i);
                lastPt = transforms[i] * vec4(position.xy, 0, 1);
            }
            gl_Position = lastPt * vec4(1.0, -1.0, 1.0, 1.0);

            tessCoord = vec2(gl_TessCoord.y, gl_TessCoord.x);
        }
        )";
static const char* fragmentShaderSrc = R"(#version 320 es
        out mediump vec4 FragColor;
        in mediump vec2 tessCoord;
        uniform sampler2D texture1;
        uniform bool enableMask;
        uniform mediump vec2 viewportSize;
        uniform sampler2D maskTexture;
        uniform mediump float opa;
        void main()
        {
            mediump vec4 maskColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
            if (enableMask) {
                mediump vec2 normalizedCoord = gl_FragCoord.xy / viewportSize;
                maskColor = texture(maskTexture, normalizedCoord);
            }

            mediump vec4 color = texture(texture1, tessCoord);
            if (enableMask && maskColor.a < 0.5) {
                discard;
            } else {
                color.a = color.a * opa;
                FragColor = vec4(color.rgba);
            }
        }
    )";
#endif
void checkGLError(const char* location)
{
    GLenum error = glGetError();
    while (error != GL_NO_ERROR)
    {
        const char* errorStr = "";
        switch (error)
        {
            case GL_INVALID_ENUM:
                errorStr = "GL_INVALID_ENUM";
                break;
            case GL_INVALID_VALUE:
                errorStr = "GL_INVALID_VALUE";
                break;
            case GL_INVALID_OPERATION:
                errorStr = "GL_INVALID_OPERATION";
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                errorStr = "GL_INVALID_FRAMEBUFFER_OPERATION";
                break;
            case GL_OUT_OF_MEMORY:
                errorStr = "GL_OUT_OF_MEMORY";
                break;
        }
        SDL_Log("OpenGL ES Error at %s: %s (0x%04X)\n", location, errorStr, error);
        error = glGetError();
    }
}
static const char* shaderTypeName(GLenum type)
{
    switch (type)
    {
        case GL_VERTEX_SHADER:
            return "vertex";
        case GL_TESS_CONTROL_SHADER:
            return "tess control";
        case GL_TESS_EVALUATION_SHADER:
            return "tess eval";
        case GL_FRAGMENT_SHADER:
            return "fragment";
        default:
            return "unknown";
    }
}
GLuint compileShader(GLenum type, const char* src)
{
    if (!TVPIsEmoteGLContextCurrent())
    {
        SDL_Log("Shader compile skipped (%s): no current GL context", shaderTypeName(type));
        return 0;
    }
    GLuint shader = glCreateShader(type);
    if (shader == 0)
    {
        SDL_Log("glCreateShader failed (%s): 0x%04X", shaderTypeName(type), glGetError());
        return 0;
    }
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::string log;
        if (logLen > 1)
        {
            log.resize(static_cast<size_t>(logLen));
            glGetShaderInfoLog(shader, logLen, nullptr, log.data());
        }
        if (log.empty())
            log = "(empty driver log)";
        SDL_Log("Shader compile error (%s): %s", shaderTypeName(type), log.c_str());
    }
    return shader;
}
GLuint createLegacyRenderProgram()
{
    if (!TVPIsEmoteGLContextCurrent())
    {
        SDL_Log("Program link skipped: no current GL context");
        return 0;
    }
    GLuint vs = compileShader(GL_VERTEX_SHADER, legacyVertexShaderSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, legacyFragmentShaderSrc);
    if (vs == 0 || fs == 0)
    {
        if (vs != 0)
            glDeleteShader(vs);
        if (fs != 0)
            glDeleteShader(fs);
        return 0;
    }
    GLuint prog = glCreateProgram();
    glBindAttribLocation(prog, 0, "aPos");
    glBindAttribLocation(prog, 1, "aTexCoord");
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint success = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success)
    {
        GLint logLen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
        std::string log;
        if (logLen > 1)
        {
            log.resize(static_cast<size_t>(logLen));
            glGetProgramInfoLog(prog, logLen, nullptr, log.data());
        }
        if (log.empty())
            log = "(empty driver log)";
        SDL_Log("Program link error (legacy): %s", log.c_str());
        glDeleteProgram(prog);
        prog = 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}
GLuint createRenderProgram()
{
    if (emoteUseLegacyGL())
        return createLegacyRenderProgram();

    if (!TVPIsEmoteGLContextCurrent())
    {
        SDL_Log("Program link skipped: no current GL context");
        return 0;
    }
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint tcs = compileShader(GL_TESS_CONTROL_SHADER, tessControlShaderSrc);
    GLuint tes = compileShader(GL_TESS_EVALUATION_SHADER, tessEvaluationShaderSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    if (vs == 0 || tcs == 0 || tes == 0 || fs == 0)
    {
        if (vs != 0)
            glDeleteShader(vs);
        if (tcs != 0)
            glDeleteShader(tcs);
        if (tes != 0)
            glDeleteShader(tes);
        if (fs != 0)
            glDeleteShader(fs);
        return 0;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, tcs);
    glAttachShader(prog, tes);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success)
    {
        GLint logLen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
        std::string log;
        if (logLen > 1)
        {
            log.resize(static_cast<size_t>(logLen));
            glGetProgramInfoLog(prog, logLen, nullptr, log.data());
        }
        if (log.empty())
            log = "(empty driver log)";
        SDL_Log("Program link error: %s", log.c_str());
        glDeleteProgram(prog);
        prog = 0;
    }
    glDeleteShader(vs);
    glDeleteShader(tcs);
    glDeleteShader(tes);
    glDeleteShader(fs);
    return prog;
}
static void emoteSetupGLState(bool clearBuffers)
{
    TVPEnsureEmoteGLContext();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    if (emoteUseLegacyGL())
        glClearDepth(0.0);
    else
        glClearDepthf(-1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!emoteUseLegacyGL())
        glBlendEquation(GL_FUNC_ADD);
    if (clearBuffers)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (emotenodeprogram == 0 || glIsProgram(emotenodeprogram) != GL_TRUE)
    {
        emotenodeprogram = createRenderProgram();
        if (!emoteUseLegacyGL() && emotenodeprogram != 0 && emotenodeVAO == 0)
            glGenVertexArrays(1, &emotenodeVAO);
    }
    if (emotenodeprogram == 0)
        return;
    glUseProgram(emotenodeprogram);
    if (!emoteUseLegacyGL())
    {
        glBindVertexArray(emotenodeVAO);
        ensurePatchVertices();
    }
}
void glBaseSet()
{
    emoteSetupGLState(true);
}
void glBaseSetWithoutClear()
{
    emoteSetupGLState(false);
}
GLuint createEmptyTexture(int width, int height)
{
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return texture;
}
GLuint createEmptyDepthTexture(int width, int height)
{
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    if (emoteUseLegacyGL())
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT,
                     GL_UNSIGNED_INT, NULL);
    }
#if defined(__APPLE__)
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT,
                     GL_FLOAT, NULL);
    }
#else
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT,
                     GL_UNSIGNED_INT, NULL);
    }
#endif

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#if defined(__APPLE__)
    if (!emoteUseLegacyGL())
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
#endif
    return texture;
}
GLuint createFBO(GLuint texture, GLuint depthtexture)
{
    GLuint result;
    glGenFramebuffers(1, &result);
    glBindFramebuffer(GL_FRAMEBUFFER, result);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthtexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        SDL_Log("Framebuffer不完整! status=0x%04X", glCheckFramebufferStatus(GL_FRAMEBUFFER));
    }

    return result;
}
GLfloat default_control_points[32] = {
    0.000000f, 0.000000f, 0.333333f, 0.000000f, 0.666667f, 0.000000f, 1.000000f, 0.000000f,
    0.000000f, 0.333333f, 0.333333f, 0.333333f, 0.666667f, 0.333333f, 1.000000f, 0.333333f,
    0.000000f, 0.666667f, 0.333333f, 0.666667f, 0.666667f, 0.666667f, 1.000000f, 0.666667f,
    0.000000f, 1.000000f, 0.333333f, 1.000000f, 0.666667f, 1.000000f, 1.000000f, 1.000000f};

#pragma endregion
} // namespace emoteplayer
