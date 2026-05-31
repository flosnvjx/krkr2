#include "EmoteGLBridge.h"

#include "cocos2d.h"

#if(CC_TARGET_PLATFORM == CC_PLATFORM_MAC) ||                               \
    (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32) ||                               \
    (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX)
#include "platform/desktop/CCGLViewImpl-desktop.h"
#include <GLFW/glfw3.h>
#endif

namespace {

bool g_emoteSwitchedGLContext = false;

#if(CC_TARGET_PLATFORM == CC_PLATFORM_MAC) ||                               \
    (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32) ||                               \
    (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX)
GLFWwindow *getEngineGLFWWindow()
{
    auto *director = cocos2d::Director::getInstance();
    if(director == nullptr)
        return nullptr;

    auto *glview = director->getOpenGLView();
    if(glview == nullptr || !glview->isOpenGLReady())
        return nullptr;

    auto *impl = dynamic_cast<cocos2d::GLViewImpl *>(glview);
    if(impl == nullptr)
        return nullptr;

    return impl->getWindow();
}
#endif

} // namespace

bool TVPIsEmoteGLContextCurrent()
{
#if(CC_TARGET_PLATFORM == CC_PLATFORM_MAC) ||                               \
    (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32) ||                               \
    (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX)
    return glfwGetCurrentContext() != nullptr;
#elif(CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID) ||                          \
    (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
    auto *director = cocos2d::Director::getInstance();
    if(director == nullptr)
        return false;
    auto *glview = director->getOpenGLView();
    return glview != nullptr && glview->isOpenGLReady();
#else
    return false;
#endif
}

bool TVPEnsureEmoteGLContext()
{
    if(TVPIsEmoteGLContextCurrent())
        return true;

#if(CC_TARGET_PLATFORM == CC_PLATFORM_MAC) ||                               \
    (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32) ||                               \
    (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX)
    GLFWwindow *window = getEngineGLFWWindow();
    if(window == nullptr)
        return false;

    glfwMakeContextCurrent(window);
    g_emoteSwitchedGLContext = glfwGetCurrentContext() != nullptr;
    return g_emoteSwitchedGLContext;
#elif(CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID) ||                          \
    (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
    auto *director = cocos2d::Director::getInstance();
    if(director == nullptr)
        return false;
    auto *glview = director->getOpenGLView();
    return glview != nullptr && glview->isOpenGLReady();
#else
    return false;
#endif
}

void TVPReleaseEmoteGLContext()
{
    if(!g_emoteSwitchedGLContext)
        return;

#if(CC_TARGET_PLATFORM == CC_PLATFORM_MAC) ||                               \
    (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32) ||                               \
    (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX)
    // Keep the engine window context current. Detaching with nullptr breaks the
    // next Cocos render pass (GL_INVALID_ENUM in saveRenderState).
    GLFWwindow *window = getEngineGLFWWindow();
    if(window != nullptr)
        glfwMakeContextCurrent(window);
#endif

    g_emoteSwitchedGLContext = false;
}
