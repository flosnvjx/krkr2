#pragma once

// Bridge motionplayer raw OpenGL calls to the KrKr2/Cocos2d-x render context.
bool TVPIsEmoteGLContextCurrent();
bool TVPEnsureEmoteGLContext();
void TVPReleaseEmoteGLContext();
