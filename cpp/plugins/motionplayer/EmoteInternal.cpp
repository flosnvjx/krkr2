#include "EmoteInternal.h"

namespace emoteplayer {

    namespace {

        float cubicBlend(float p0, float p1, float p2, float p3, float t) {
            const float mt = 1.0f - t;
            return mt * mt * mt * p0 + 3.0f * mt * mt * t * p1 +
                3.0f * mt * t * t * p2 + t * t * t * p3;
        }

        void bezierSurface2D(float u, float v, const float *controlPts,
                             float &outX, float &outY) {
            float curveX[4];
            float curveY[4];
            for(int row = 0; row < 4; ++row) {
                const float *base = controlPts + row * 8;
                curveX[row] = cubicBlend(base[0], base[2], base[4], base[6], u);
                curveY[row] = cubicBlend(base[1], base[3], base[5], base[7], u);
            }
            outX = cubicBlend(curveX[0], curveX[1], curveX[2], curveX[3], v);
            outY = cubicBlend(curveY[0], curveY[1], curveY[2], curveY[3], v);
        }

    } // namespace

    void evaluatePatchPosition(float u, float v, int surfaceCount,
                               const float *transforms,
                               const float *const *controlPoints, float &outX,
                               float &outY, float &outZ, float &outW) {
        float px = u;
        float py = v;
        float pz = 0.0f;
        float pw = 0.0f;
        for(int i = 0; i < surfaceCount; ++i) {
            if(i > 0) {
                const float oldX = px;
                const float oldY = py;
                px = 1.0f - (oldY / 2.0f + 0.5f);
                py = 0.5f + oldX / 2.0f;
                pz = 0.0f;
                pw = 0.0f;
            }
            const float *cps =
                (controlPoints != nullptr && controlPoints[i] != nullptr)
                ? controlPoints[i]
                : default_control_points;
            float sx = 0.0f;
            float sy = 0.0f;
            bezierSurface2D(px, py, cps, sx, sy);
            const float *m = transforms + i * 16;
            const float tx = m[0] * sx + m[4] * sy + m[8] * 0.0f + m[12] * 1.0f;
            const float ty = m[1] * sx + m[5] * sy + m[9] * 0.0f + m[13] * 1.0f;
            const float tz =
                m[2] * sx + m[6] * sy + m[10] * 0.0f + m[14] * 1.0f;
            const float tw =
                m[3] * sx + m[7] * sy + m[11] * 0.0f + m[15] * 1.0f;
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

    float default_control_points[32] = {
        0.000000f, 0.000000f, 0.333333f, 0.000000f, 0.666667f, 0.000000f,
        1.000000f, 0.000000f, 0.000000f, 0.333333f, 0.333333f, 0.333333f,
        0.666667f, 0.333333f, 1.000000f, 0.333333f, 0.000000f, 0.666667f,
        0.333333f, 0.666667f, 0.666667f, 0.666667f, 1.000000f, 0.666667f,
        0.000000f, 1.000000f, 0.333333f, 1.000000f, 0.666667f, 1.000000f,
        1.000000f, 1.000000f
    };

} // namespace emoteplayer
