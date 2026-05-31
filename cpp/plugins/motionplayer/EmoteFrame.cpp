#include "emotefile.h"
#include "EmoteInternal.h"


#include <sstream>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <cmath>
#include <cassert>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#define GLM_ASSERT_VALID(matrix)                                               \
    do {                                                                       \
        const glm::mat4 &m = (matrix);                                         \
        for(int i = 0; i < 4; ++i) {                                           \
            for(int j = 0; j < 4; ++j) {                                       \
                assert(!std::isnan(m[i][j]) && "矩阵包含NaN值");               \
                assert(!std::isinf(m[i][j]) && "矩阵包含无穷大值");            \
            }                                                                  \
        }                                                                      \
    } while(0)

using namespace PSB;

namespace emoteplayer {
    emoteframe::emoteframe(emotefile *filePtr, uint32_t startOffset) :
        _filePtr(filePtr) {
        // dic
        std::map<std::string, uint32_t> _rootData;
        filePtr->parseObject(_rootData, startOffset);
        // time type
        auto timeIt = _rootData.find("time");
        if(timeIt != _rootData.end())
            filePtr->parseReal(time, timeIt->second);
        int64_t tmp = 0;
        auto typeIt = _rootData.find("type");
        if(typeIt != _rootData.end() &&
           filePtr->parseNumber(tmp, typeIt->second))
            type = static_cast<int8_t>(tmp);
        // content
        auto it = _rootData.find("content");
        if(it != _rootData.end()) {
            hasContent = true;
            filePtr->updateSyncTime(time);
            filePtr->parseObject(_rootData, it->second);
            // coord angle s z zcc
            it = _rootData.find("coord");
            if(it != _rootData.end()) {
                std::vector<uint32_t> _listOffset;
                filePtr->parseList(_listOffset, it->second);
                if(_listOffset.size() == 3) {
                    filePtr->parseReal(coordX, _listOffset[0]);
                    filePtr->parseReal(coordY, _listOffset[1]);
                    filePtr->parseReal(coordZ, _listOffset[2]);
                    filePtr->updateZMax(coordZ);
                }
            }
            it = _rootData.find("angle");
            if(it != _rootData.end()) {
                filePtr->parseReal(angle, it->second);
            }
            it = _rootData.find("sx");
            if(it != _rootData.end()) {
                filePtr->parseReal(sx, it->second);
            }
            it = _rootData.find("sy");
            if(it != _rootData.end()) {
                filePtr->parseReal(sy, it->second);
            }
            it = _rootData.find("zx");
            if(it != _rootData.end()) {
                filePtr->parseReal(zx, it->second);
            }
            it = _rootData.find("zy");
            if(it != _rootData.end()) {
                filePtr->parseReal(zy, it->second);
            }
            it = _rootData.find("ox");
            if(it != _rootData.end()) {
                filePtr->parseReal(ox, it->second);
            }
            it = _rootData.find("oy");
            if(it != _rootData.end()) {
                filePtr->parseReal(oy, it->second);
            }
            it = _rootData.find("zcc");
            if(it != _rootData.end()) {
                std::map<std::string, uint32_t> zcc_map;
                if(filePtr->parseObject(zcc_map, it->second)) {
                    haszcc = true;
                    // c
                    std::vector<uint32_t> zcc_list;
                    filePtr->parseList(zcc_list, zcc_map["c"]);
                    if(zcc_list.size() == 2) {
                        for(int i = 0; i < 2; i++)
                            filePtr->parseReal(zcc_c[i], zcc_list[i]);
                    }
                    // x
                    zcc_list.clear();
                    filePtr->parseList(zcc_list, zcc_map["x"]);
                    if(zcc_list.size() == 4) {
                        for(int i = 0; i < 4; i++)
                            filePtr->parseReal(zcc_x[i], zcc_list[i]);
                    }
                    // y
                    zcc_list.clear();
                    filePtr->parseList(zcc_list, zcc_map["y"]);
                    if(zcc_list.size() == 4) {
                        for(int i = 0; i < 4; i++)
                            filePtr->parseReal(zcc_y[i], zcc_list[i]);
                    }
                }
            }
            // ccc
            it = _rootData.find("ccc");
            if(it != _rootData.end()) {
                std::map<std::string, uint32_t> ccc_map;
                if(filePtr->parseObject(ccc_map, it->second)) {
                    hasccc = true;
                    // c
                    std::vector<uint32_t> ccc_list;
                    filePtr->parseList(ccc_list, ccc_map["c"]);
                    if(ccc_list.size() == 2) {
                        for(int i = 0; i < 2; i++)
                            filePtr->parseReal(ccc_c[i], ccc_list[i]);
                    }
                    // x
                    ccc_list.clear();
                    filePtr->parseList(ccc_list, ccc_map["x"]);
                    if(ccc_list.size() == 4) {
                        for(int i = 0; i < 4; i++)
                            filePtr->parseReal(ccc_x[i], ccc_list[i]);
                    }
                    // y
                    ccc_list.clear();
                    filePtr->parseList(ccc_list, ccc_map["y"]);
                    if(ccc_list.size() == 4) {
                        for(int i = 0; i < 4; i++)
                            filePtr->parseReal(ccc_y[i], ccc_list[i]);
                    }
                }
            }
            // src
            if(filePtr->isKrkr) {
                it = _rootData.find("src");
                if(it != _rootData.end())
                    filePtr->parseString(src, it->second);
            } else {
                it = _rootData.find("src");
                if(it == _rootData.end()) // 说明此时是layout
                {
                    src = "layout";
                } else // 否则按照旧规则直接拼接吧
                {
                    std::string tmp;
                    const auto srcOffset = it->second;
                    // 初始为src，然后再看看是否为motion
                    src = "src/";
                    it = _rootData.find("motion");
                    if(it != _rootData.end()) {
                        src = "motion/";
                        // 获取timeoffset
                        std::map<std::string, uint32_t> tmpMap;
                        if(filePtr->parseObject(tmpMap, it->second)) {
                            it = tmpMap.find("timeOffset");
                            if(it != tmpMap.end()) {
                                filePtr->parseReal(timeOffset, it->second);
                            }
                        }
                    }
                    filePtr->parseString(tmp, srcOffset);
                    // 还有一个blank情形
                    if(strcmp(tmp.c_str(), "blank") == 0) {
                        src = "";
                    }
                    src += tmp;
                    auto iconIt = _rootData.find("icon");
                    if(iconIt != _rootData.end()) {
                        filePtr->parseString(tmp, iconIt->second);
                        src += "/" + tmp;
                    }
                    src.erase(std::remove(src.begin(), src.end(), '\0'),
                              src.end());
                }
            }
            // mask
            it = _rootData.find("mask");
            if(it != _rootData.end())
                filePtr->parseNumber(mask, it->second);
            // bm
            it = _rootData.find("bm");
            if(it != _rootData.end()) {
                filePtr->parseNumber(bm, it->second);
            }
            // opa
            it = _rootData.find("opa");
            if(it != _rootData.end()) {
                filePtr->parseReal(opa, it->second);
            }
            // mesh
            auto it = _rootData.find("mesh");
            if(it != _rootData.end()) {
                filePtr->parseObject(_rootData, it->second);
                // bp
                std::vector<uint32_t> meshOffset;
                filePtr->parseList(meshOffset, _rootData["bp"]);
                if(meshOffset.size() == 32) {
                    hasbp = true;
                    for(int i = 0; i < 32; i++)
                        filePtr->parseReal(bp[i], meshOffset[i]);
                }
                // cc
                auto it = _rootData.find("cc");
                if(it != _rootData.end()) {
                    if(filePtr->parseObject(_rootData, it->second)) {
                        hascc = true;
                        // c
                        meshOffset.clear();
                        filePtr->parseList(meshOffset, _rootData["c"]);
                        if(meshOffset.size() == 2) {
                            for(int i = 0; i < 2; i++)
                                filePtr->parseReal(cc_c[i], meshOffset[i]);
                        }
                        // x
                        meshOffset.clear();
                        filePtr->parseList(meshOffset, _rootData["x"]);
                        if(meshOffset.size() == 4) {
                            for(int i = 0; i < 4; i++)
                                filePtr->parseReal(cc_x[i], meshOffset[i]);
                        }
                        // y
                        meshOffset.clear();
                        filePtr->parseList(meshOffset, _rootData["y"]);
                        if(meshOffset.size() == 4) {
                            for(int i = 0; i < 4; i++)
                                filePtr->parseReal(cc_y[i], meshOffset[i]);
                        }
                    }
                }
            }
        }
    }
    emoteframe::~emoteframe() {}
} // namespace emoteplayer
