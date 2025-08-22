# ====================== CMakeLists.txt ======================
cmake_minimum_required(VERSION 3.16)
project(JobLens LANGUAGES CXX)

# ----------------------------------------------
# 1. 基本配置
# ----------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------------------------
# 2. 依赖查找
# ----------------------------------------------
include(FetchContent)

# 统一开关：若希望强制使用系统包，可手动置 OFF
option(JOBLENS_AUTO_DEPS "Auto-download missing 3rd-party libs" ON)

macro(fetch_if_missing pkg url)
    if(NOT ${pkg}_FOUND AND JOBLENS_AUTO_DEPS)
        FetchContent_Declare(${pkg}
            GIT_REPOSITORY ${url}
        )
        FetchContent_MakeAvailable(${pkg})
    endif()
endmacro()

# 1) fmt
find_package(fmt QUIET)
fetch_if_missing(fmt https://github.com/fmtlib/fmt.git)

# 2) spdlog（header-only 也能工作）
find_package(spdlog QUIET)
fetch_if_missing(spdlog https://github.com/gabime/spdlog.git)

# 3) yaml-cpp
find_package(yaml-cpp QUIET)
fetch_if_missing(yaml-cpp https://github.com/jbeder/yaml-cpp.git)

# 4) Howard Hinnant date
find_package(date QUIET)
fetch_if_missing(date https://github.com/HowardHinnant/date.git)

# ----------------------------------------------
# 3. 变量：把路径集中定义，方便后续修改
# ----------------------------------------------
set(PROFILER_INC_DIR "${CMAKE_SOURCE_DIR}/include")
set(PROFILER_SRC_DIR "${CMAKE_SOURCE_DIR}/src")

# ----------------------------------------------
# 4. 头文件
#    使用 target_include_directories 而不是全局 include_directories，
#    更 modern、更可控。
# ----------------------------------------------
# 注：nlohmann/json 是 header-only，已经在 utils 目录里，无需 find_package。
#     如果后续改为系统安装的版本，可改为 find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(date REQUIRED)

# ----------------------------------------------
# 5. 收集所有源文件
#    这里用 GLOB_RECURSE 方便一次性获取；若你更喜欢手动列出，也可改成 set(...)
# ----------------------------------------------
file(GLOB_RECURSE PROFILER_SOURCES
     CONFIGURE_DEPENDS
     "${PROFILER_SRC_DIR}/*.cpp"
)

# ----------------------------------------------
# 6. 定义可执行目标
# ----------------------------------------------
add_executable(JobLens
    ${PROFILER_SOURCES}
)

# ----------------------------------------------
# 7. 给目标添加头文件搜索路径
# ----------------------------------------------
target_include_directories(JobLens
    PUBLIC
        ${PROFILER_INC_DIR}
        fmt::fmt 
        spdlog::spdlog
        date::date
        yaml-cpp::yaml-cpp
)

# ----------------------------------------------
# 8. 链接系统库
# ----------------------------------------------
target_link_libraries(JobLens
    PRIVATE
        fmt::fmt 
        spdlog::spdlog
        date::date
        yaml-cpp::yaml-cpp
        # 示例：如果使用 libbpf
        # ${LIBBPF_LIBRARIES}
)

# ----------------------------------------------
# 9. 安装规则（可选）
# ----------------------------------------------
install(TARGETS JobLens
        RUNTIME DESTINATION bin)

# 如果你想把 include 目录也安装出去：
# install(DIRECTORY ${PROFILER_INC_DIR}
#         DESTINATION include)