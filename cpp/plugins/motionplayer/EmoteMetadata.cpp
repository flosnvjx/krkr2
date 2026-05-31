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
    emoteattrcomp::emoteattrcomp(emotefile *filePtr, uint32_t startOffset) {
        // dic
        std::map<std::string, uint32_t> _rootData;
        filePtr->parseObject(_rootData, startOffset);
        // lable
        filePtr->parseString(lable, _rootData["label"]);
        // remove_data
        if(filePtr->parseObject(_rootData, _rootData["data"])) {
            std::vector<uint32_t> _tmpVec;
            if(filePtr->parseList(_tmpVec, _rootData["remove"])) {
                for(auto vecItm : _tmpVec) {
                    emoteattrcompRemoveItem _tmpitm;
                    if(filePtr->parseObject(_rootData, vecItm)) {
                        filePtr->parseReal(_tmpitm.value, _rootData["value"]);
                        if(filePtr->parseObject(_rootData, _rootData["id"])) {
                            filePtr->parseString(_tmpitm.chara,
                                                 _rootData["chara"]);
                            filePtr->parseString(_tmpitm.motion,
                                                 _rootData["motion"]);
                            filePtr->parseString(_tmpitm.layer,
                                                 _rootData["layer"]);
                        }
                    }
                    data_remove.push_back(_tmpitm);
                }
            }
        }
    }
    emoteattrcomp::~emoteattrcomp() {}

    emoteselect::emoteselect(emotefile *filePtr, uint32_t startOffset) :
        _filePtr(filePtr) {
        // dic
        std::map<std::string, uint32_t> _rootData;
        filePtr->parseObject(_rootData, startOffset);
        // lable
        filePtr->parseString(lable, _rootData["label"]);
        // optionList
        std::vector<uint32_t> _tmpList;
        if(filePtr->parseList(_tmpList, _rootData["optionList"])) {
            for(auto attcom : _tmpList) {
                if(filePtr->parseObject(_rootData, attcom)) {
                    emoteselectItem _tmpItm;
                    filePtr->parseString(_tmpItm.label, _rootData["label"]);
                    filePtr->parseReal(_tmpItm.offValue, _rootData["offValue"]);
                    filePtr->parseReal(_tmpItm.onValue, _rootData["onValue"]);
                    selectItem.push_back(_tmpItm);
                }
            }
        }
    }
    emoteselect::~emoteselect() {}
    bool emoteselect::selectValue(int32_t opt) {
        if(opt < 0 || opt >= selectItem.size())
            return false;
        for(int i = 0; i < selectItem.size(); i++) {
            if(i == opt)
                _filePtr->setVariable(selectItem.at(i).label,
                                      selectItem.at(i).onValue);
            else
                _filePtr->setVariable(selectItem.at(i).label,
                                      selectItem.at(i).offValue);
        }
        return true;
    }

    emotemetadata::emotemetadata(emotefile *filePtr, uint32_t startOffset) :
        _filePtr(filePtr) {
        // dic
        std::map<std::string, uint32_t> _rootData;
        if(!filePtr->parseObject(_rootData, startOffset)) {
            filePtr->isMotion = true;
            return;
        }
        // label
        std::map<std::string, uint32_t> _base;
        filePtr->parseObject(_base, _rootData["base"]);
        filePtr->parseString(chara, _base["chara"]);
        filePtr->parseString(motion, _base["motion"]);

        // attrcomp
        std::vector<uint32_t> _tmpList;
        auto attrItm = _rootData.find("attrcomp");
        if(attrItm != _rootData.end()) {
            if(filePtr->parseList(_tmpList, attrItm->second)) {
                for(auto attcom : _tmpList) {
                    emoteattrcomp *_tmpattr =
                        new emoteattrcomp(filePtr, attcom);
                    _attrcomp.push_back(_tmpattr);
                }
            }
        }
        // selectorControl
        _tmpList.clear();
        attrItm = _rootData.find("selectorControl");
        if(attrItm != _rootData.end()) {
            if(filePtr->parseList(_tmpList, attrItm->second)) {
                for(auto attcom : _tmpList) {
                    emoteselect *_tmpattr = new emoteselect(filePtr, attcom);
                    _selectorControl.push_back(_tmpattr);
                }
            }
        }
        // mirrorControl
        attrItm = _rootData.find("mirror");
        if(attrItm != _rootData.end()) {
            tjs_int64 isNeed = 0;
            filePtr->parseNumber(isNeed, attrItm->second);
            if(isNeed == 1) {
                filePtr->isMirror = true;
                // if (filePtr->parseObject(_base, _rootData["mirrorControl"]))
                //{
                //     auto it = _base.find("variableMatchList");
                //     if (it != _base.end())
                //     {
                //         std::vector<uint32_t> tmpList;
                //         if (filePtr->parseList(tmpList, it->second))
                //         {
                //             for (auto mtc : tmpList)
                //             {
                //                 std::string tmpStr;
                //                 filePtr->parseString(tmpStr, mtc);
                //                 _mirrorControl.push_back(tmpStr);
                //             }
                //         }
                //     }
                // }
            }
        }

        // timelineControl
        _tmpList.clear();
        filePtr->parseList(_tmpList, _rootData["timelineControl"]);
        for(auto itm : _tmpList) {
            emotetimeline *tmp = new emotetimeline(filePtr, itm);
            _timelineControl.push_back(tmp);
        }

        // variableList Simplified
        _tmpList.clear();
        filePtr->parseList(_tmpList, _rootData["variableList"]);
        for(auto itm : _tmpList) {
            filePtr->parseObject(_base, itm);
            auto it = _base.find("label");
            if(it != _base.end()) {
                std::string key;
                filePtr->parseString(key, it->second);
                _varList.insert(std::pair<std::string, float>(key, 0.0f));
            }
        }

        // eyeControl Simplified
        _tmpList.clear();
        filePtr->parseList(_tmpList, _rootData["eyeControl"]);
        for(auto itm : _tmpList) {
            filePtr->parseObject(_base, itm);
            eyeControl *tmp = new eyeControl();
            int64_t tmpNum = 0;
            auto it = _base.find("label");
            if(it != _base.end()) {
                filePtr->parseString(tmp->label, it->second);
            }
            it = _base.find("beginFrame");
            if(it != _base.end()) {
                filePtr->parseNumber(tmpNum, it->second);
                tmp->beginFrame = static_cast<float>(tmpNum);
            }
            it = _base.find("endFrame");
            if(it != _base.end()) {
                filePtr->parseNumber(tmpNum, it->second);
                tmp->endFrame = static_cast<float>(tmpNum);
            } else {
                tmp->endFrame = tmp->beginFrame;
            }
            it = _base.find("blinkFrameCount");
            if(it != _base.end()) {
                filePtr->parseNumber(tmpNum, it->second);
                tmp->blinkFrameCount = static_cast<float>(tmpNum);
            }
            it = _base.find("blinkIntervalMax");
            if(it != _base.end()) {
                filePtr->parseNumber(tmpNum, it->second);
                tmp->blinkIntervalMax = static_cast<float>(tmpNum);
            }
            it = _base.find("blinkIntervalMin");
            if(it != _base.end()) {
                filePtr->parseNumber(tmpNum, it->second);
                tmp->blinkIntervalMin = static_cast<float>(tmpNum);
            }
            tmp->uid = std::uniform_int_distribution<int32_t>(
                tmp->blinkIntervalMin, tmp->blinkIntervalMax);
            _eyeControl.push_back(tmp);
        }
        _tmpList.clear();
        filePtr->parseList(_tmpList, _rootData["eyebrowControl"]);
        for(auto itm : _tmpList) {
            filePtr->parseObject(_base, itm);
            eyebrowControl *tmp = new eyebrowControl();
            int64_t tmpNum = 0;
            auto it = _base.find("label");
            if(it != _base.end()) {
                filePtr->parseString(tmp->label, it->second);
            }
            it = _base.find("beginFrame");
            if(it != _base.end()) {
                filePtr->parseNumber(tmpNum, it->second);
                tmp->beginFrame = static_cast<float>(tmpNum);
            }
            it = _base.find("endFrame");
            if(it != _base.end()) {
                filePtr->parseNumber(tmpNum, it->second);
                tmp->endFrame = static_cast<float>(tmpNum);
            } else {
                tmp->endFrame = tmp->beginFrame;
            }
            _eyebrowControl.push_back(tmp);
        }

        // bustControl
        _tmpList.clear();
        filePtr->parseList(_tmpList, _rootData["bustControl"]);
        for(auto itm : _tmpList) {
            bustControl *tmp = new bustControl(filePtr, itm);
            _bustControl.push_back(tmp);
        }
        // hairControl
        _tmpList.clear();
        filePtr->parseList(_tmpList, _rootData["hairControl"]);
        for(auto itm : _tmpList) {
            uniControl *tmp = new uniControl(filePtr, itm);
            _hairControl.push_back(tmp);
        }
        // partsControl
        _tmpList.clear();
        filePtr->parseList(_tmpList, _rootData["partsControl"]);
        for(auto itm : _tmpList) {
            uniControl *tmp = new uniControl(filePtr, itm);
            _partsControl.push_back(tmp);
        }
    }
    emotemetadata::~emotemetadata() {
        for(auto itm : _attrcomp) {
            if(itm != nullptr)
                delete itm;
        }
        for(auto itm : _selectorControl) {
            if(itm != nullptr)
                delete itm;
        }
        for(auto itm : _timelineControl) {
            if(itm != nullptr)
                delete itm;
        }
        for(auto itm : _eyeControl) {
            if(itm != nullptr)
                delete itm;
        }
        for(auto itm : _eyebrowControl) {
            if(itm != nullptr)
                delete itm;
        }
        for(auto itm : _bustControl) {
            if(itm != nullptr)
                delete itm;
        }
        for(auto itm : _hairControl) {
            if(itm != nullptr)
                delete itm;
        }
        for(auto itm : _partsControl) {
            if(itm != nullptr)
                delete itm;
        }
    }

    emoteicon::emoteicon(emotefile *filePtr, uint32_t startOffset) :
        _filePtr(filePtr) {
        // dic
        std::map<std::string, uint32_t> _rootData;
        filePtr->parseObject(_rootData, startOffset);
        // base
        filePtr->parseReal(originX, _rootData["originX"]);
        filePtr->parseReal(originY, _rootData["originY"]);
        filePtr->parseReal(width, _rootData["width"]);
        filePtr->parseReal(height, _rootData["height"]);
        if(filePtr->isKrkr) {
            // compress type
            auto it = _rootData.find("compress");
            if(it == _rootData.end()) {
                compress = "none";
            } else {
                filePtr->parseString(compress, it->second);
            }
            // pixel id
            filePtr->parseResource(pixel, _rootData["pixel"]);
            // is pal?
            it = _rootData.find("pal");
            if(it != _rootData.end()) {
                type = "pal";
                filePtr->parseResource(pal, it->second);
            }
        } else {
            // left/top
            filePtr->parseReal(left, _rootData["left"]);
            filePtr->parseReal(top, _rootData["top"]);
        }

        // clip
        auto it = _rootData.find("clip");
        if(it != _rootData.end()) {
            filePtr->parseObject(_rootData, it->second);
            filePtr->parseReal(_clip.bottom, _rootData["bottom"]);
            filePtr->parseReal(_clip.left, _rootData["left"]);
            filePtr->parseReal(_clip.right, _rootData["right"]);
            filePtr->parseReal(_clip.top, _rootData["top"]);
        }
    }
    emoteicon::~emoteicon() {}

    emotesource::emotesource(emotefile *filePtr, uint32_t startOffset) :
        _filePtr(filePtr) {
        // dic
        std::map<std::string, uint32_t> _rootData;
        filePtr->parseObject(_rootData, startOffset);
        // type
        int64_t tmp;
        filePtr->parseNumber(tmp, _rootData["type"]);
        type = static_cast<int8_t>(tmp);
        // icon
        std::map<std::string, uint32_t> _motion;
        filePtr->parseObject(_motion, _rootData["icon"]);
        for(auto _obj : _motion) {
            emoteicon *tmp = new emoteicon(filePtr, _obj.second);
            icon.insert(std::pair<std::string, emoteicon *>(_obj.first, tmp));
        }
        // texture
        if(!filePtr->isKrkr) {
            if(filePtr->parseObject(_rootData, _rootData["texture"])) {
                std::string intType;
                double width = 0;
                double height = 0;
                int32_t pixel = -1;
                filePtr->parseReal(width, _rootData["width"]);
                filePtr->parseReal(height, _rootData["height"]);
                filePtr->parseString(intType, _rootData["type"]);
                filePtr->parseResource(pixel, _rootData["pixel"]);
                // 子类赋值
                for(auto _obj : icon) {
                    _obj.second->type = intType;
                    _obj.second->pixel = pixel;
                    _obj.second->texWidth = width;
                    _obj.second->texHeight = height;
                }
            }
        }
    }
    emotesource::~emotesource() {
        for(auto ic : icon) {
            if(ic.second != nullptr)
                delete ic.second;
        }
    }
} // namespace emoteplayer
