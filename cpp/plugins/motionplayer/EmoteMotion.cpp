#include "emotefile.h"
#include "EmoteInternal.h"

#include "LayerIntf.h"


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

#define GLM_ASSERT_VALID(matrix) \
    do \
    { \
        const glm::mat4& m = (matrix); \
        for (int i = 0; i < 4; ++i) \
        { \
            for (int j = 0; j < 4; ++j) \
            { \
                assert(!std::isnan(m[i][j]) && "矩阵包含NaN值"); \
                assert(!std::isinf(m[i][j]) && "矩阵包含无穷大值"); \
            } \
        } \
    } while (0)

using namespace PSB;

namespace emoteplayer
{
    emotemotion::emotemotion(emotefile* filePtr, uint32_t startOffset) : _filePtr(filePtr)
    {
        // dic
        std::map<std::string, uint32_t> _rootData;
        filePtr->parseObject(_rootData, startOffset);
        // time
        filePtr->parseNumber(lastTime, _rootData["lastTime"]);
        filePtr->parseNumber(loopTime, _rootData["loopTime"]);
        // layer
        std::vector<uint32_t> _layer;
        filePtr->parseList(_layer, _rootData["layer"]);
        for (size_t i = 0; i < _layer.size(); i++)
        {
            emotenode* tmp = new emotenode(this, nullptr, nodeList, filePtr, _layer.at(i));
            layer.push_back(tmp);
        }

        // priority
        _layer.clear();
        filePtr->parseList(_layer, _rootData["priority"]);
        if (_layer.size() > 0)
        {
            std::map<std::string, uint32_t> _priority;
            filePtr->parseObject(_priority, _layer.at(0));
            auto it = _priority.find("content");
            if (it != _priority.end())
            {
                _layer.clear();
                filePtr->parseList(_layer, _priority["content"]);
                for (auto idx : _layer)
                {
                    int64_t tmp;
                    filePtr->parseNumber(tmp, idx);
                    priority.push_back(static_cast<int32_t>(tmp));
                }
            }
        }
        // rearrange
        std::vector<emotenode*> result;
        for (auto node_idx = priority.begin(); node_idx != priority.end(); ++node_idx)
        {
            if (*node_idx >= nodeList.size())
                continue;

            result.push_back(nodeList.at(*node_idx));
        }
        nodeList = result;

        // parameter
        _layer.clear();
        filePtr->parseList(_layer, _rootData["parameter"]);
        for (auto varItem : _layer)
        {
            std::map<std::string, uint32_t> varMap;
            filePtr->parseObject(varMap, varItem);
            emoteVar* var = new emoteVar();
            int64_t tmp = 0;
            filePtr->parseNumber(tmp, varMap["rangeBegin"]);
            var->rangeBegin = static_cast<int32_t>(tmp);
            filePtr->parseNumber(tmp, varMap["rangeEnd"]);
            var->rangeEnd = static_cast<int32_t>(tmp);
            filePtr->parseNumber(tmp, varMap["division"]);
            var->division = static_cast<int32_t>(tmp);
            filePtr->parseString(var->id, varMap["id"]);
            parameter.push_back(var);
        }
        // parameter to child
        auto it = _rootData.find("parameterize");
        if (it != _rootData.end())
        {
            int64_t tmp = 0;
            if (filePtr->parseNumber(tmp, it->second))
            {
                isParameterize = true;
                parameterIdx = static_cast<int32_t>(tmp);
            }
        }
        if (isParameterize)
        {
            for (auto itmNode : nodeList)
            {
                if (!itmNode->isParameterize)
                {
                    itmNode->isParameterize = true;
                    itmNode->parameterIdx = parameterIdx;
                }
            }
        }

        // selfSyncTime
        for (auto _node : nodeList)
        {
            for (auto _frame : _node->frameList)
            {
                if (_frame != nullptr && _frame->hasContent)
                {
                    if (_frame->time > selfSyncTime)
                        selfSyncTime = _frame->time;
                }
            }
        }
    }
    emotemotion::~emotemotion()
    {
        for (auto lay : layer)
        {
            if (lay != nullptr)
                delete lay;
        }
        for (auto para : parameter)
        {
            if (para != nullptr)
                delete para;
        }
    }
    float emotemotion::getTickByIdx(int32_t parameterIdx)
    {
        if (parameterIdx >= parameter.size() || parameterIdx < 0)
            return -1.0f;
        float currVal = 0;
        if (_filePtr->getTickByName(parameter.at(parameterIdx)->id, currVal))
        {
            parameter.at(parameterIdx)->currVal = currVal;
        }
        return parameter.at(parameterIdx)->transToTick();
    }
    emotenode* emotemotion::getNodeByName(const std::string& name)
    {
        for (auto node : nodeList)
        {
            if (node == nullptr)
                continue;
            if (strcmp(name.c_str(), node->label.c_str()) == 0) // 仅靠一个label，合理吗？
            {
                return node;
            }
        }
        return nullptr;
    }
    void emotemotion::progress(float tick, std::vector<emoteRender>& renderList, emotelimit lim)
    {
        // 树状便利，构建节点的渲染结构
        for (auto ch : layer)
        {
            renderMethod.clear();
            renderMethod = renderList;
            // 蒙版是否穿过motion，没啥好的依据，就随便猜一个了
            if (renderMethod.size() > 0 && ch->type == 2)
            {
                renderMethod.at(0).hasStencil = false;
                renderMethod.at(0).layerNode.clear();
            }
            ch->progress(tick, renderMethod, lim);
        }
    }
    void emotemotion::draw(GLuint targetFbo, emotelimit lim, GLuint exFbo, GLuint exTex)
    {
        std::vector<emotenode*> result, resultEx;
        if (_filePtr->isMotion)
        {
            // motion则是先处理非motion节点，然后再处理motion节点
            for (auto node_idx = nodeList.rbegin(); node_idx != nodeList.rend(); ++node_idx)
            {
                if ((*node_idx)->emot == nullptr)
                    result.push_back(*node_idx);
                else
                    resultEx.push_back(*node_idx);
            }
        }
        else
        {
            // 先展开所有的motion情形
            std::vector<emotenode*> stack(nodeList.begin(), nodeList.end());
            while (!stack.empty())
            {
                emotenode* current = stack.back();
                stack.pop_back();

                if (current->emot == nullptr)
                {
                    result.push_back(current);
                }
                else
                {
                    for (auto it = current->emot->nodeList.begin();
                         it != current->emot->nodeList.end(); ++it)
                    {
                        stack.push_back(*it);
                    }
                }
            }

            // 再根据lastZ进行校准
            std::stable_sort(result.begin(), result.end(), [](emotenode* a, emotenode* b)
                             { return a->getCurrentRenderZ() < b->getCurrentRenderZ(); });
        }

        // 遍历绘制节点
        for (auto currNode : result)
        {
            if (currNode != nullptr)
            {
                currNode->draw(targetFbo, lim, exFbo, exTex);
            }
        }
        for (auto currNode : resultEx)
        {
            if (currNode != nullptr)
            {
                currNode->draw(targetFbo, lim, exFbo, exTex);
            }
        }
    }
    void emotemotion::drawToLayer(tTJSNI_BaseLayer* layer, emotelimit lim, tTJSNI_BaseLayer* maskLayer)
    {
        std::vector<emotenode*> result, resultEx;
        if (_filePtr->isMotion)
        {
            for (auto node_idx = nodeList.rbegin(); node_idx != nodeList.rend(); ++node_idx)
            {
                if ((*node_idx)->emot == nullptr)
                    result.push_back(*node_idx);
                else
                    resultEx.push_back(*node_idx);
            }
        }
        else
        {
            std::vector<emotenode*> stack(nodeList.begin(), nodeList.end());
            while (!stack.empty())
            {
                emotenode* current = stack.back();
                stack.pop_back();

                if (current->emot == nullptr)
                {
                    result.push_back(current);
                }
                else
                {
                    for (auto it = current->emot->nodeList.begin(); it != current->emot->nodeList.end();
                         ++it)
                    {
                        stack.push_back(*it);
                    }
                }
            }

            std::stable_sort(result.begin(), result.end(), [](emotenode* a, emotenode* b)
                             { return a->getCurrentRenderZ() < b->getCurrentRenderZ(); });
        }

        tTJSNI_BaseLayer* maskTarget = maskLayer;
        for (auto currNode : result)
        {
            if (currNode != nullptr)
                currNode->drawToLayer(layer, lim, maskTarget);
        }
        for (auto currNode : resultEx)
        {
            if (currNode != nullptr)
                currNode->drawToLayer(layer, lim, maskTarget);
        }
    }
    void emotemotion::drawToBitmap(iTVPBaseBitmap* target, const tTVPRect& targetClip, emotelimit lim,
                                  tTJSNI_BaseLayer* maskLayer)
    {
        std::vector<emotenode*> result, resultEx;
        if (_filePtr->isMotion)
        {
            for (auto node_idx = nodeList.rbegin(); node_idx != nodeList.rend(); ++node_idx)
            {
                if ((*node_idx)->emot == nullptr)
                    result.push_back(*node_idx);
                else
                    resultEx.push_back(*node_idx);
            }
        }
        else
        {
            std::vector<emotenode*> stack(nodeList.begin(), nodeList.end());
            while (!stack.empty())
            {
                emotenode* current = stack.back();
                stack.pop_back();

                if (current->emot == nullptr)
                {
                    result.push_back(current);
                }
                else
                {
                    for (auto it = current->emot->nodeList.begin(); it != current->emot->nodeList.end();
                         ++it)
                    {
                        stack.push_back(*it);
                    }
                }
            }

            std::stable_sort(result.begin(), result.end(), [](emotenode* a, emotenode* b)
                             { return a->getCurrentRenderZ() < b->getCurrentRenderZ(); });
        }

        tTJSNI_BaseLayer* maskTarget = maskLayer;
        for (auto currNode : result)
        {
            if (currNode != nullptr)
                currNode->drawToBitmap(target, targetClip, lim, maskTarget);
        }
        for (auto currNode : resultEx)
        {
            if (currNode != nullptr)
                currNode->drawToBitmap(target, targetClip, lim, maskTarget);
        }
    }

    emoteobject::emoteobject(emotefile* filePtr, uint32_t startOffset) : _filePtr(filePtr)
    {
        // dic
        std::map<std::string, uint32_t> _rootData;
        filePtr->parseObject(_rootData, startOffset);
        // type
        int64_t tmp;
        filePtr->parseNumber(tmp, _rootData["type"]);
        type = static_cast<int8_t>(tmp);
        // motion
        std::map<std::string, uint32_t> _motion;
        filePtr->parseObject(_motion, _rootData["motion"]);
        for (auto _obj : _motion)
        {
            emotemotion* tmp = new emotemotion(filePtr, _obj.second);
            motion.insert(std::pair<std::string, emotemotion*>(_obj.first, tmp));
        }
    }
    emoteobject::~emoteobject()
    {
        for (auto mtn : motion)
        {
            if (mtn.second != nullptr)
                delete mtn.second;
        }
    }
    emoteVar* emoteobject::findVarByName(const std::string& name)
    {
        for (auto mtn : motion)
        {
            for (auto var : mtn.second->parameter)
            {
                if (var->id == name)
                {
                    return var;
                }
            }
        }
        return nullptr;
    }
} // namespace emoteplayer
