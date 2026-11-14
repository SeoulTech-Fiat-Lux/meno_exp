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
# 동결 태그를 새로 만들면 이 해시와 sfml 템플릿의 SFML 태그를 함께 확인할 것.
FetchContent_Declare(
        meno
        GIT_REPOSITORY https://github.com/SeoulTech-Fiat-Lux/Meno.git
        GIT_TAG        3a1b67c0eeb42075292e72ef1b65e889f7c4f610 # v0.2.0
)
# meno는 내부에서 SFML 3.1.0을 정적 라이브러리로 받아 빌드한다(sfml 템플릿과 같은 조건).
FetchContent_MakeAvailable(meno)

add_executable(game main.cpp)
target_link_libraries(game PRIVATE meno::meno)

# 두 조건이 같은 에셋을 쓰도록 공통 폴더를 실행 파일 옆 assets/로 복사한다.
set(MINIGAME_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets" CACHE PATH "공통 에셋 폴더")
add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${MINIGAME_ASSETS_DIR}" "$<TARGET_FILE_DIR:game>/assets")
