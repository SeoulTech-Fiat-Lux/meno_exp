# 성능 측정 프로그램 (SeoulTech-Fiat-Lux/Meno#30). Meno 저장소는 동결했으므로(46f8934) 고치지 않고,
# llm-comparison/templates/meno와 같은 방식으로 같은 커밋을 받아 공개 API만으로 잰다.
#
#     cmake -S . -B build -DCMAKE_BUILD_TYPE=Release [-DBENCH_ENABLE_TRACY=ON]
#     cmake --build build && ./build/meno_bench --target meno-object --count 1000 --out out.csv
cmake_minimum_required(VERSION 3.28)
project(meno_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BENCH_ENABLE_TRACY "Tracy 계측을 켠다. 병목 분석 전용이고, 이 빌드의 수치는 측정 표에 쓰지 않는다." OFF)

include(FetchContent)

# meno는 내부에서 SFML을 태그(3.1.0)로 받는다. FetchContent는 먼저 선언한 것을 쓰므로, 여기서 같은 이름으로
# 커밋 해시를 먼저 선언해 meno와 sfml 대상이 같은 SFML 커밋으로 빌드되게 한다(templates/meno와 같음).
FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        8835b6b955c96fa1ad1278a8dae6f05c4143effc # 3.1.0
        SYSTEM
        EXCLUDE_FROM_ALL
)
FetchContent_Declare(
        meno
        GIT_REPOSITORY https://github.com/SeoulTech-Fiat-Lux/Meno.git
        GIT_TAG        46f8934f0f7754a7e09033c4413b6a5222d4493d # v0.2.0, 동결 커밋 (#29와 같음)
)
FetchContent_MakeAvailable(meno)

add_executable(meno_bench main.cpp)
# meno는 SFML을 PRIVATE로 링크하므로 sfml 대상을 위해 SFML::Graphics를 직접 링크한다.
target_link_libraries(meno_bench PRIVATE meno::meno SFML::Graphics)
if(NOT MSVC)
    target_compile_options(meno_bench PRIVATE -Wall -Wextra)
endif()

if(BENCH_ENABLE_TRACY)
    # Tracy의 CMake는 TRACY_ENABLE이 기본 OFF다. 켜지 않으면 매크로가 비어 캡처가 빈다.
    set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
    # 캡처 도구(benchmark/tracy.py, bench.toml [tracy].version)와 같은 버전이어야 연결된다.
    FetchContent_Declare(
            tracy
            GIT_REPOSITORY https://github.com/wolfpld/tracy.git
            GIT_TAG        f2a4f2529e7e71844c9e434b877770a21ecf7587 # v0.15.0
            SYSTEM
            EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(tracy)
    target_link_libraries(meno_bench PRIVATE Tracy::TracyClient)
    target_compile_definitions(meno_bench PRIVATE BENCH_ENABLE_TRACY)
endif()
