# SFML 3.1 조건 빌드 템플릿. LLM은 main.cpp만 작성하며, 이 파일은 고치지 않는다.
#
#     cmake -S . -B build -DMINIGAME_ASSETS_DIR=<공통 assets 폴더>
#     cmake --build build --target game
#     cd build && ./game        # 작업 디렉터리에 assets/가 있어야 한다
cmake_minimum_required(VERSION 3.28)
project(minigame_sfml LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)

# meno v0.2.0이 내부에서 쓰는 SFML 설정과 같게 맞춘다: 3.1.0, 정적 빌드, network 모듈 제외.
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_DOC      OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK  OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS   OFF)

# 태그는 옮겨질 수 있으므로 커밋 해시로 고정한다 (meno 템플릿과 같은 해시. 얕은 clone은 해시를 받지 못해 쓰지 않는다).
FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        8835b6b955c96fa1ad1278a8dae6f05c4143effc # 3.1.0
        SYSTEM
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(SFML)

add_executable(game main.cpp)
# Graphics는 Window·System을 함께 끌어온다.
target_link_libraries(game PRIVATE SFML::Graphics SFML::Audio)

# 두 조건이 같은 에셋을 쓰도록 공통 폴더를 실행 파일 옆 assets/로 복사한다.
set(MINIGAME_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets" CACHE PATH "공통 에셋 폴더")
add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${MINIGAME_ASSETS_DIR}" "$<TARGET_FILE_DIR:game>/assets")
