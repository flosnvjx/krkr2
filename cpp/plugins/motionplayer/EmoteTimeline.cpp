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
    emoteTimeVarFrame::emoteTimeVarFrame(emotefile *filePtr,
                                         uint32_t startOffset) {
        // dic
        std::map<std::string, uint32_t> _rootData;
        filePtr->parseObject(_rootData, startOffset);
        // time type
        filePtr->parseReal(time, _rootData["time"]);
        int64_t tmp;
        filePtr->parseNumber(tmp, _rootData["type"]);
        type = static_cast<int8_t>(tmp);
        // content
        auto it = _rootData.find("content");
        if(it != _rootData.end()) {
            if(filePtr->parseObject(_rootData, it->second)) {
                hasContent = true;
                // easing value
                it = _rootData.find("easing");
                if(it != _rootData.end()) {
                    filePtr->parseReal(easing, it->second);
                }
                it = _rootData.find("value");
                if(it != _rootData.end()) {
                    filePtr->parseReal(value, it->second);
                }
            }
        }
    }
    emoteTimeVarFrame::~emoteTimeVarFrame() {}
    emoteTimeVar::emoteTimeVar(emotefile *filePtr, uint32_t startOffset) {
        // dic
        std::map<std::string, uint32_t> _rootData;
        filePtr->parseObject(_rootData, startOffset);
        // label
        filePtr->parseString(label, _rootData["label"]);
        // frameList
        std::vector<uint32_t> _tmpList;
        filePtr->parseList(_tmpList, _rootData["frameList"]);
        for(size_t i = 0; i < _tmpList.size(); i++) {
            emoteTimeVarFrame *tmp =
                new emoteTimeVarFrame(filePtr, _tmpList.at(i));
            frameList.push_back(tmp);
        }
    }
    emoteTimeVar::~emoteTimeVar() {
        for(auto itm : frameList) {
            if(itm != nullptr)
                delete itm;
        }
    }
    emotetimeline::emotetimeline(emotefile *filePtr, uint32_t startOffset) {
        // dic
        std::map<std::string, uint32_t> _rootData;
        filePtr->parseObject(_rootData, startOffset);
        // frameinfo
        int64_t tmp = 0;
        filePtr->parseNumber(tmp, _rootData["lastTime"]);
        lastTime = static_cast<int32_t>(tmp);
        filePtr->parseNumber(tmp, _rootData["loopBegin"]);
        loopBegin = static_cast<int32_t>(tmp);
        filePtr->parseNumber(tmp, _rootData["loopEnd"]);
        loopEnd = static_cast<int32_t>(tmp);
        auto diffIt = _rootData.find("diff");
        if(diffIt != _rootData.end() &&
           filePtr->parseNumber(tmp, diffIt->second))
            diff = static_cast<int8_t>(tmp);

        // label
        filePtr->parseString(label, _rootData["label"]);
        // variableList
        std::vector<uint32_t> _tmpList;
        filePtr->parseList(_tmpList, _rootData["variableList"]);
        for(size_t i = 0; i < _tmpList.size(); i++) {
            emoteTimeVar *tmp = new emoteTimeVar(filePtr, _tmpList.at(i));
            variableList.push_back(tmp);
        }
    }
    emotetimeline::~emotetimeline() {
        for(auto itm : variableList) {
            if(itm != nullptr)
                delete itm;
        }
    }
} // namespace emoteplayer
