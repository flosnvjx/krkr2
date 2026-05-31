#include "EmoteTVPRenderer.h"
#include "EmoteInternal.h"

#include "LayerBitmapIntf.h"
#include "LayerIntf.h"
#include "RenderManager.h"

#include <array>
#include <cmath>
#include <vector>

namespace emoteplayer {
    namespace {

// CPU AffineBlt mesh path: each cell does 2 blits. divTotal=50 → ~1k+
// blits/icon; on a full-screen offscreen bitmap that feels like a hang; CPU
// Bezier tessellation is costly.
#if defined(__ANDROID__) || defined(__APPLE__)
        constexpr int kMaxTvpMeshDivisionTotal = 12;
#else
        constexpr int kMaxTvpMeshDivisionTotal = 16;
#endif

        void computeMeshDivision(int meshDivision, int iconW, int iconH,
                                 tjs_int &divX, tjs_int &divY) {
            int divTotal = meshDivision;
            if(divTotal > kMaxTvpMeshDivisionTotal)
                divTotal = kMaxTvpMeshDivisionTotal;
            if(divTotal < 1)
                divTotal = 4;
            divX = static_cast<tjs_int>(static_cast<double>(divTotal) * iconW /
                                        (iconW + iconH)) +
                1;
            divY = divTotal - divX + 2;
            if(divX < 2)
                divX = 2;
            if(divY < 2)
                divY = 2;
        }

        bool isFiniteMeshPoint(const tTVPPointD &pt) {
            return std::isfinite(pt.x) && std::isfinite(pt.y);
        }

        bool triangleDestBBox(const tTVPPointD *tri, tjs_int &left,
                              tjs_int &top, tjs_int &right, tjs_int &bottom) {
            if(!isFiniteMeshPoint(tri[0]) || !isFiniteMeshPoint(tri[1]) ||
               !isFiniteMeshPoint(tri[2]))
                return false;

            double minX = tri[0].x;
            double maxX = tri[0].x;
            double minY = tri[0].y;
            double maxY = tri[0].y;
            for(int i = 1; i < 3; ++i) {
                minX = std::min(minX, tri[i].x);
                maxX = std::max(maxX, tri[i].x);
                minY = std::min(minY, tri[i].y);
                maxY = std::max(maxY, tri[i].y);
            }

            left = static_cast<tjs_int>(std::floor(minX));
            top = static_cast<tjs_int>(std::floor(minY));
            right = static_cast<tjs_int>(std::ceil(maxX));
            bottom = static_cast<tjs_int>(std::ceil(maxY));
            return right > left && bottom > top;
        }

        tTVPRect intersectClipRect(const tTVPRect &a, const tTVPRect &b) {
            tTVPRect out(a.left, a.top, a.right, a.bottom);
            out.clip(b);
            return out;
        }

        tTVPRect meshDestClip(const std::vector<tTVPPointD> &meshPoints,
                              const tTVPRect &targetClip) {
            bool hasPoint = false;
            tjs_int left = 0;
            tjs_int top = 0;
            tjs_int right = 0;
            tjs_int bottom = 0;
            for(const auto &pt : meshPoints) {
                if(!isFiniteMeshPoint(pt))
                    continue;
                const tjs_int px = static_cast<tjs_int>(std::floor(pt.x));
                const tjs_int py = static_cast<tjs_int>(std::floor(pt.y));
                const tjs_int px2 = static_cast<tjs_int>(std::ceil(pt.x));
                const tjs_int py2 = static_cast<tjs_int>(std::ceil(pt.y));
                if(!hasPoint) {
                    left = px;
                    top = py;
                    right = px2;
                    bottom = py2;
                    hasPoint = true;
                    continue;
                }
                left = std::min(left, px);
                top = std::min(top, py);
                right = std::max(right, px2);
                bottom = std::max(bottom, py2);
            }
            if(!hasPoint || right <= left || bottom <= top)
                return tTVPRect();
            return intersectClipRect(tTVPRect(left, top, right, bottom),
                                     targetClip);
        }

        // Map evaluatePatchPosition clip output to TVP layer pixels (top-left
        // origin). Matches glm::ortho + gl_Position Y flip + viewport used by
        // emotenode::draw.
        void clipToLayerPoint(float px, float py, float pw,
                              const emotelimit &lim, tTVPPointD &out) {
            const float w = (pw != 0.0f) ? pw : 1.0f;
            const double ndcX = static_cast<double>(px / w);
            const double ndcY = static_cast<double>(py / w);
            out.x = static_cast<double>(lim.originX) +
                (ndcX * 0.5 + 0.5) * static_cast<double>(lim.width);
            out.y = static_cast<double>(lim.originY) +
                (1.0 - (ndcY * 0.5 + 0.5)) * static_cast<double>(lim.height);
        }

        tjs_int opacity255(float opa) {
            return static_cast<tjs_int>(std::max(0.0f, std::min(1.0f, opa)) *
                                        255.0f);
        }

        tTVPBBBltMethod bltMethodFromEmote(int8_t blendMode) {
            switch(blendMode & 0x0f) {
                case 1:
                    return bmPsAdditive;
                case 2:
                case 5:
                    return bmPsSubtractive;
                case 3:
                    return bmPsMultiplicative;
                case 4:
                    return bmPsScreen;
                default:
                    return bmAlpha;
            }
        }

        tTVPPointD offsetPoint(const tTVPPointD &pt, double xOffset,
                               double yOffset) {
            return { pt.x + xOffset, pt.y + yOffset };
        }

        std::array<tTVPPointD, 3>
        affineTargetTriangle(int surfaceCount, const float *transforms,
                             const float *const *controlPoints,
                             const emotelimit &lim, double xOffset,
                             double yOffset) {
            tTVPPointD p0{};
            tTVPPointD p1{};
            tTVPPointD p2{};
            float px = 0.0f;
            float py = 0.0f;
            float pz = 0.0f;
            float pw = 0.0f;
            evaluatePatchPosition(0.0f, 0.0f, surfaceCount, transforms,
                                  controlPoints, px, py, pz, pw);
            clipToLayerPoint(px, py, pw, lim, p0);
            evaluatePatchPosition(1.0f, 0.0f, surfaceCount, transforms,
                                  controlPoints, px, py, pz, pw);
            clipToLayerPoint(px, py, pw, lim, p1);
            evaluatePatchPosition(0.0f, 1.0f, surfaceCount, transforms,
                                  controlPoints, px, py, pz, pw);
            clipToLayerPoint(px, py, pw, lim, p2);
            return { { offsetPoint(p0, xOffset, yOffset),
                       offsetPoint(p1, xOffset, yOffset),
                       offsetPoint(p2, xOffset, yOffset) } };
        }

        std::array<tTVPPointD, 16> buildBezierControlPointsInLayer(
            int surfaceCount, const float *transforms,
            const float *const *controlPoints, const emotelimit &lim,
            double xOffset, double yOffset) {
            const float *cps = default_control_points;
            if(controlPoints != nullptr) {
                for(int i = surfaceCount - 1; i >= 0; --i) {
                    if(controlPoints[i] != nullptr &&
                       controlPoints[i] != default_control_points) {
                        cps = controlPoints[i];
                        break;
                    }
                }
            }

            std::array<tTVPPointD, 16> out{};
            for(int i = 0; i < 16; ++i) {
                float px = 0.0f;
                float py = 0.0f;
                float pz = 0.0f;
                float pw = 0.0f;
                evaluatePatchPosition(cps[i * 2], cps[i * 2 + 1], surfaceCount,
                                      transforms, controlPoints, px, py, pz,
                                      pw);
                tTVPPointD layerPt{};
                clipToLayerPoint(px, py, pw, lim, layerPt);
                out[static_cast<size_t>(i)] =
                    offsetPoint(layerPt, xOffset, yOffset);
            }
            return out;
        }

        void meshCopyToBitmap(iTVPBaseBitmap *target,
                              const tTVPRect &targetClip,
                              iTVPBaseBitmap *sourceBitmap,
                              const tTVPRect &sourceRect,
                              const std::vector<tTVPPointD> &meshPoints,
                              tjs_int divX, tjs_int divY,
                              tTVPBBBltMethod method, tjs_int opa) {
            if(target == nullptr || sourceBitmap == nullptr || divX < 2 ||
               divY < 2 ||
               static_cast<size_t>(divX) * static_cast<size_t>(divY) >
                   meshPoints.size() ||
               opa <= 0)
                return;

            const tTVPRect destClip = meshDestClip(meshPoints, targetClip);
            if(destClip.right <= destClip.left ||
               destClip.bottom <= destClip.top)
                return;

            const double srcLeft = sourceRect.left;
            const double srcTop = sourceRect.top;
            const double srcWidth = sourceRect.get_width();
            const double srcHeight = sourceRect.get_height();
            const int64_t maxTriPixels =
                static_cast<int64_t>(destClip.get_width()) *
                static_cast<int64_t>(destClip.get_height()) * 4;

            for(tjs_int y = 0; y < divY - 1; ++y) {
                const double v0 =
                    static_cast<double>(y) / static_cast<double>(divY - 1);
                const double v1 =
                    static_cast<double>(y + 1) / static_cast<double>(divY - 1);
                for(tjs_int x = 0; x < divX - 1; ++x) {
                    const double u0 =
                        static_cast<double>(x) / static_cast<double>(divX - 1);
                    const double u1 = static_cast<double>(x + 1) /
                        static_cast<double>(divX - 1);

                    const auto &p0 =
                        meshPoints[static_cast<size_t>(y) * divX + x];
                    const auto &p1 =
                        meshPoints[static_cast<size_t>(y) * divX + x + 1];
                    const auto &p2 =
                        meshPoints[static_cast<size_t>(y + 1) * divX + x];
                    const auto &p3 =
                        meshPoints[static_cast<size_t>(y + 1) * divX + x + 1];

                    const tTVPRect cellRect(
                        static_cast<tjs_int>(
                            std::floor(srcLeft + srcWidth * u0)),
                        static_cast<tjs_int>(
                            std::floor(srcTop + srcHeight * v0)),
                        static_cast<tjs_int>(
                            std::ceil(srcLeft + srcWidth * u1)),
                        static_cast<tjs_int>(
                            std::ceil(srcTop + srcHeight * v1)));
                    if(cellRect.right <= cellRect.left ||
                       cellRect.bottom <= cellRect.top)
                        continue;

                    const tTVPPointD upperTriangle[3] = { p0, p1, p2 };
                    const tTVPPointD lowerTriangle[3] = { p3, p2, p1 };
                    tjs_int triLeft = 0;
                    tjs_int triTop = 0;
                    tjs_int triRight = 0;
                    tjs_int triBottom = 0;
                    auto blitTriangle = [&](const tTVPPointD *tri) {
                        if(!triangleDestBBox(tri, triLeft, triTop, triRight,
                                             triBottom))
                            return;
                        const int64_t triArea =
                            static_cast<int64_t>(triRight - triLeft) *
                            static_cast<int64_t>(triBottom - triTop);
                        if(triArea <= 0 || triArea > maxTriPixels)
                            return;
                        const tTVPRect triClip = intersectClipRect(
                            tTVPRect(triLeft, triTop, triRight, triBottom),
                            destClip);
                        if(triClip.right <= triClip.left ||
                           triClip.bottom <= triClip.top)
                            return;
                        tTVPRect updateRect{};
                        target->AffineBlt(triClip, sourceBitmap, cellRect, tri,
                                          method, opa, &updateRect, false,
                                          stNearest, false, 0);
                    };
                    blitTriangle(upperTriangle);
                    blitTriangle(lowerTriangle);
                }
            }
        }

        void drawPatchToBitmap(iTVPBaseBitmap *target,
                               const tTVPRect &targetClip,
                               const emotelimit &lim, int surfaceCount,
                               const float *transforms,
                               const float *const *controlPoints,
                               iTVPBaseBitmap *sourceBitmap, int iconWidth,
                               int iconHeight, float opa, int8_t blendMode,
                               uint8_t meshDivision, bool useBezierMesh) {
            if(target == nullptr || sourceBitmap == nullptr ||
               surfaceCount <= 0 || transforms == nullptr || iconWidth <= 0 ||
               iconHeight <= 0 || opa <= 0.0f)
                return;

            const tTVPRect sourceRect(0, 0, iconWidth, iconHeight);
            const tjs_int opa255 = opacity255(opa);
            if(opa255 <= 0)
                return;

            const tTVPBBBltMethod method = bltMethodFromEmote(blendMode);

            const double xOffset = -0.5 - static_cast<double>(targetClip.left);
            const double yOffset = -0.5 - static_cast<double>(targetClip.top);

            tjs_int divX = 2;
            tjs_int divY = 2;
            computeMeshDivision(meshDivision, iconWidth, iconHeight, divX,
                                divY);

            if(useBezierMesh) {
                const auto controlPointsInLayer =
                    buildBezierControlPointsInLayer(surfaceCount, transforms,
                                                    controlPoints, lim, xOffset,
                                                    yOffset);
                const auto cubicBlendD = [](double p0, double p1, double p2,
                                            double p3, double t) {
                    const double mt = 1.0 - t;
                    return mt * mt * mt * p0 + 3.0 * mt * mt * t * p1 +
                        3.0 * mt * t * t * p2 + t * t * t * p3;
                };
                const auto samplePatch = [&](double u, double v) -> tTVPPointD {
                    tTVPPointD curve[4];
                    for(int row = 0; row < 4; ++row) {
                        const tTVPPointD *cp =
                            &controlPointsInLayer[static_cast<size_t>(row) * 4];
                        curve[row].x =
                            cubicBlendD(cp[0].x, cp[1].x, cp[2].x, cp[3].x, u);
                        curve[row].y =
                            cubicBlendD(cp[0].y, cp[1].y, cp[2].y, cp[3].y, u);
                    }
                    return { cubicBlendD(curve[0].x, curve[1].x, curve[2].x,
                                         curve[3].x, v),
                             cubicBlendD(curve[0].y, curve[1].y, curve[2].y,
                                         curve[3].y, v) };
                };

                std::vector<tTVPPointD> meshPoints(static_cast<size_t>(divX) *
                                                   static_cast<size_t>(divY));
                for(tjs_int y = 0; y < divY; ++y) {
                    const double v = divY > 1
                        ? static_cast<double>(y) / static_cast<double>(divY - 1)
                        : 0.0;
                    for(tjs_int x = 0; x < divX; ++x) {
                        const double u = divX > 1 ? static_cast<double>(x) /
                                static_cast<double>(divX - 1)
                                                  : 0.0;
                        meshPoints[static_cast<size_t>(y) * divX + x] =
                            samplePatch(u, v);
                    }
                }
                meshCopyToBitmap(target, targetClip, sourceBitmap, sourceRect,
                                 meshPoints, divX, divY, method, opa255);
                return;
            }

            const auto dstTri = affineTargetTriangle(
                surfaceCount, transforms, controlPoints, lim, xOffset, yOffset);
            tTVPRect updateRect{};
            target->AffineBlt(targetClip, sourceBitmap, sourceRect,
                              dstTri.data(), method, opa255, &updateRect, false,
                              stNearest, false, 0);
        }

    } // namespace

    bool emoteUseTVPRenderer() { return true; }

    void emoteDrawPatchToBitmap(iTVPBaseBitmap *target,
                                const tTVPRect &targetClip,
                                const emotelimit &lim, int surfaceCount,
                                const float *transforms,
                                const float *const *controlPoints,
                                iTVPBaseBitmap *sourceBitmap, int iconWidth,
                                int iconHeight, float opa, int8_t blendMode,
                                uint8_t meshDivision, bool useBezierMesh) {
        drawPatchToBitmap(target, targetClip, lim, surfaceCount, transforms,
                          controlPoints, sourceBitmap, iconWidth, iconHeight,
                          opa, blendMode, meshDivision, useBezierMesh);
    }

    void emoteDrawPatchToLayer(tTJSNI_BaseLayer *layer, const emotelimit &lim,
                               int surfaceCount, const float *transforms,
                               const float *const *controlPoints,
                               iTVPBaseBitmap *sourceBitmap, int iconWidth,
                               int iconHeight, float opa, int8_t blendMode,
                               uint8_t meshDivision, bool useBezierMesh) {
        if(layer == nullptr)
            return;
        tTVPBaseTexture *mainImage = layer->GetMainImage();
        if(mainImage == nullptr)
            return;
        const tTVPRect targetClip(layer->GetClipLeft(), layer->GetClipTop(),
                                  layer->GetClipLeft() + layer->GetClipWidth(),
                                  layer->GetClipTop() + layer->GetClipHeight());
        drawPatchToBitmap(mainImage, targetClip, lim, surfaceCount, transforms,
                          controlPoints, sourceBitmap, iconWidth, iconHeight,
                          opa, blendMode, meshDivision, useBezierMesh);
    }

} // namespace emoteplayer
