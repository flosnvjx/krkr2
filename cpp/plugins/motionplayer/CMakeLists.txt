cmake_minimum_required(VERSION 3.19)
project(motionplayer)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME}
    PUBLIC
        main.cpp
    PRIVATE
        EmoteFileCore.cpp
        EmoteInternal.cpp
        EmoteFrame.cpp
        EmoteMetadata.cpp
        EmoteMotion.cpp
        EmotePhysics.cpp
        EmoteNode.cpp
        EmoteTimeline.cpp
        EmoteTVPRenderer.cpp
        emoteplayerclass.cpp

)

target_link_libraries(${PROJECT_NAME} PRIVATE krkr2plugin)
