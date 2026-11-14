# meno 조건 빌드 템플릿. LLM은 main.cpp만 작성하며, 이 파일은 고치지 않는다.
#
#     cmake -S . -B build -DMINIGAME_ASSETS_DIR=<공통 assets 폴더>
#     cmake --build build --target game
#     cd build && ./game        # 작업 디렉터리에 assets/가 있어야 한다
cmake_minimum_required(VERSION 3.28)
project(minigame_meno LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)

# 실험 동결 버전. 태그는 옮겨질 수 있으므로 커밋 해시로 고정한다.
# 동결 태그를 새로 만들면 이 해시를 바꾼다 (harness/freeze.py가 contexts/manifest.json과 대조한다).
FetchContent_Declare(
        meno
        GIT_REPOSITORY https://github.com/SeoulTech-Fiat-Lux/Meno.git
        GIT_TAG        46f8934f0f7754a7e09033c4413b6a5222d4493d # v0.2.0 (태그가 3a1b67c에서 옮겨진 뒤의 커밋)
)
# meno는 내부에서 SFML을 태그(3.1.0)로 받는다. FetchContent는 먼저 선언한 것을 쓰므로, 여기서 같은 이름으로
# 커밋 해시를 먼저 선언해 두 조건이 같은 SFML 커밋으로 빌드되게 한다. 옵션(SYSTEM, EXCLUDE_FROM_ALL)은 meno와 같다.
# sfml 템플릿의 해시와 같아야 한다 (freeze.py가 확인).
FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        8835b6b955c96fa1ad1278a8dae6f05c4143effc # 3.1.0
        SYSTEM
        EXCLUDE_FROM_ALL
)
# meno가 SFML을 정적 라이브러리로 빌드한다(sfml 템플릿과 같은 조건).
FetchContent_MakeAvailable(meno)

add_executable(game main.cpp)
target_link_libraries(game PRIVATE meno::meno)

# 두 조건이 같은 에셋을 쓰도록 공통 폴더를 실행 파일 옆 assets/로 복사한다.
set(MINIGAME_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets" CACHE PATH "공통 에셋 폴더")
add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${MINIGAME_ASSETS_DIR}" "$<TARGET_FILE_DIR:game>/assets")
