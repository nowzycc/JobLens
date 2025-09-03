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

find_package(PkgConfig  REQUIRED)
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)

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
        PkgConfig::CURL
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
        PkgConfig::CURL
)

# ----------------------------------------------
# 9. 安装规则（可选）
# ----------------------------------------------
install(TARGETS JobLens
        RUNTIME DESTINATION bin)

# 如果你想把 include 目录也安装出去：
# install(DIRECTORY ${PROFILER_INC_DIR}
#         DESTINATION include)

# ====================== 追加到原 CMakeLists.txt 末尾 ======================

# ----------------------------------------------
# 10. CPack 通用配置
# ----------------------------------------------
set(CPACK_PACKAGE_NAME "JobLens")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance job profiler")
set(CPACK_PACKAGE_VENDOR "nowzycc")
set(CPACK_PACKAGE_CONTACT "wangzhenyuan@ihep.ac.com")
# set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")  # 如有
# set(CPACK_RESOURCE_FILE_README  "${CMAKE_SOURCE_DIR}/README.md")

# 生成两种格式：tar.gz + deb
set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "")   # 我们自带 .so，留空即可
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")

# 安装前缀：打包时所有文件会以 /opt/JobLens 为根
# set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/JobLens")

# ----------------------------------------------
# 11. 自动收集并安装运行时 .so
# ----------------------------------------------
# 11-1 准备 staging 目录
set(DEPS_STAGING_DIR "${CMAKE_BINARY_DIR}/runtime_deps")  # 临时收集目录
file(MAKE_DIRECTORY "${DEPS_STAGING_DIR}")

# 11-2 获取目标所有运行时依赖（排除系统白名单）
add_custom_target(collect_deps ALL
    COMMAND ${CMAKE_COMMAND} -E env
        LD_LIBRARY_PATH=${DEPS_STAGING_DIR}:$ENV{LD_LIBRARY_PATH}
        bash "${CMAKE_SOURCE_DIR}/scripts/collect_libs.sh"
            $<TARGET_FILE:JobLens>          # 主可执行文件
            ${DEPS_STAGING_DIR}             # 输出目录
    COMMENT "Collecting runtime .so ..."
    VERBATIM
)

# 11-3 把收集到的 .so 安装到 lib/ 目录，并自动加入包
install(DIRECTORY ${DEPS_STAGING_DIR}/
        DESTINATION lib
        FILES_MATCHING PATTERN "*.so*")

# 11-4 让可执行文件优先找同目录 lib/ 下的 .so
#     注意：必须在 install() 之后，否则 install(TARGETS) 会覆盖 rpath
set_target_properties(JobLens PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"   # 相对可执行文件 bin/JobLens -> ../lib
)

# ----------------------------------------------
# 12. 启用 CPack
# ----------------------------------------------
include(CPack)
