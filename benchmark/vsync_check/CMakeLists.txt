# 수직 동기화 확인용 프로그램. vsync_check.py가 빌드하고 돌린다.
# SFML은 llm-comparison/templates/sfml과 같은 커밋(3.1.0)으로 고정한다.
cmake_minimum_required(VERSION 3.28)
project(vsync_check LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_DOC      OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK  OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_AUDIO    OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS   OFF)
FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        8835b6b955c96fa1ad1278a8dae6f05c4143effc # 3.1.0
        SYSTEM
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(SFML)

add_executable(vsync_check main.cpp)
target_link_libraries(vsync_check PRIVATE SFML::Graphics)
