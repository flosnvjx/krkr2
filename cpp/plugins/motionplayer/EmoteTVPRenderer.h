#pragma once

#include "emotefile.h"

class tTJSNI_BaseLayer;
class iTVPBaseBitmap;

namespace emoteplayer {

bool emoteUseTVPRenderer();

void emoteDrawPatchToBitmap(iTVPBaseBitmap* target, const tTVPRect& targetClip,
                            const emotelimit& lim, int surfaceCount, const float* transforms,
                            const float* const* controlPoints, iTVPBaseBitmap* sourceBitmap,
                            int iconWidth, int iconHeight, float opa, int8_t blendMode,
                            uint8_t meshDivision, bool useBezierMesh);

void emoteDrawPatchToLayer(tTJSNI_BaseLayer* layer, const emotelimit& lim, int surfaceCount,
                           const float* transforms, const float* const* controlPoints,
                           iTVPBaseBitmap* sourceBitmap, int iconWidth, int iconHeight,
                           float opa, int8_t blendMode, uint8_t meshDivision, bool useBezierMesh);

} // namespace emoteplayer
