#pragma once

namespace emoteplayer {

    extern float default_control_points[32];

    void evaluatePatchPosition(float u, float v, int surfaceCount,
                               const float *transforms,
                               const float *const *controlPoints, float &outX,
                               float &outY, float &outZ, float &outW);

} // namespace emoteplayer
