cmake_minimum_required(VERSION 3.16)
project(silog_system)

# 启用测试
enable_testing()

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含公共头文件目录
include_directories(${PROJECT_SOURCE_DIR}/interface)

# 引入 GoogleTest - 优先使用系统安装版本，否则使用 FetchContent
find_package(GTest QUIET)

if(GTest_FOUND)
    message(STATUS "Found system-installed GoogleTest")
    # CMake 3.20+ 使用 GTest::gtest_main, 旧版本使用 GTest::Main
    if(NOT TARGET GTest::gtest_main)
        if(TARGET gtest_main)
            add_library(GTest::gtest_main ALIAS gtest_main)
        endif()
    endif()
else()
    message(STATUS "System GoogleTest not found, using FetchContent")

    include(FetchContent)

    # 设置本地缓存目录（下载后会被缓存到这里）
    set(FETCHCONTENT_BASE_DIR "${PROJECT_SOURCE_DIR}/.cache" CACHE PATH "FetchContent cache directory" FORCE)

    # 检查本地缓存目录（FetchContent 下载后的位置是 googletest-src）
    set(GTEST_CACHE_DIR "${FETCHCONTENT_BASE_DIR}/googletest-src")
    if(EXISTS "${GTEST_CACHE_DIR}/CMakeLists.txt")
        message(STATUS "Using cached GoogleTest from ${GTEST_CACHE_DIR}")
        set(FETCHCONTENT_SOURCE_DIR_GOOGLETEST "${GTEST_CACHE_DIR}" CACHE PATH "Googletest source directory" FORCE)
    else()
        message(STATUS "GoogleTest cache not found, will download from GitHub")
    endif()

    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.14.0
      GIT_SHALLOW TRUE
      GIT_PROGRESS TRUE
    )

    # 对于 Windows: 防止覆盖父项目的编译器/链接器设置
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # 创建别名以便统一链接
    if(TARGET gtest AND NOT TARGET GTest::gtest)
        add_library(GTest::gtest ALIAS gtest)
    endif()
    if(TARGET gtest_main AND NOT TARGET GTest::gtest_main)
        add_library(GTest::gtest_main ALIAS gtest_main)
    endif()
endif()

# 选项：启用 ASan (AddressSanitizer)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

# 选项：启用 TSan (ThreadSanitizer)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

# 选项：启用 LCOV 覆盖率
option(ENABLE_LCOV "Enable LCOV code coverage" OFF)

# 选项：启用测试发现（显示单个测试用例）
option(ENABLE_TEST_DISCOVERY "Enable individual test case discovery" OFF)

# 添加编译选项
add_compile_options(-Wall -Wextra -Werror -DDEBUG)
# add_compile_options(-DSILOG_EXE)
add_compile_definitions(_GNU_SOURCE)

# ASan 配置
if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# TSan 配置
if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

# LCOV 覆盖率配置
if(ENABLE_LCOV)
    message(STATUS "LCOV code coverage enabled")
    add_compile_options(-fprofile-arcs -ftest-coverage -O0 -g)
    add_link_options(--coverage)
    # 链接 gcov 库
    link_libraries(gcov)
endif()

# 添加子目录
add_subdirectory(silog_comm)
add_subdirectory(silog_logger)
add_subdirectory(silog_dae)
add_subdirectory(silog_cat)
add_subdirectory(silog_exe)
add_subdirectory(tests)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# LCOV 覆盖率目标
if(ENABLE_LCOV)
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)

    if(LCOV_PATH AND GENHTML_PATH)
        # 创建覆盖率报告目录
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/coverage)

        # 初始化覆盖率数据
        add_custom_target(lcov-init
            COMMAND ${LCOV_PATH} --capture --initial --directory ${CMAKE_BINARY_DIR}
                    --output-file ${CMAKE_BINARY_DIR}/coverage/lcov.base
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Initializing LCOV coverage data"
        )

        # 捕获测试后的覆盖率数据
        add_custom_target(lcov-capture
            COMMAND ${LCOV_PATH} --capture --directory ${CMAKE_BINARY_DIR}
                    --output-file ${CMAKE_BINARY_DIR}/coverage/lcov.info
            COMMAND ${LCOV_PATH} --add-tracefile ${CMAKE_BINARY_DIR}/coverage/lcov.base
                    --add-tracefile ${CMAKE_BINARY_DIR}/coverage/lcov.info
                    --output-file ${CMAKE_BINARY_DIR}/coverage/lcov.total
            COMMAND ${LCOV_PATH} --remove ${CMAKE_BINARY_DIR}/coverage/lcov.total
                    '/usr/*' '${CMAKE_BINARY_DIR}/_deps/*'
                    --output-file ${CMAKE_BINARY_DIR}/coverage/lcov.filtered
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Capturing LCOV coverage data"
        )

        # 生成 HTML 报告
        add_custom_target(lcov-report
            COMMAND ${GENHTML_PATH} ${CMAKE_BINARY_DIR}/coverage/lcov.filtered
                    --output-directory ${CMAKE_BINARY_DIR}/coverage/html
                    --title "SiLog Coverage Report"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating LCOV HTML report"
        )

        # 一键生成完整报告
        add_custom_target(lcov
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target lcov-init
            COMMAND ctest --output-on-failure || true
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target lcov-capture
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target lcov-report
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating full LCOV coverage report"
        )
    else()
        message(WARNING "lcov or genhtml not found. Coverage targets will not be available.")
    endif()
endif()

# 格式化命令 find . -name "CMakeLists.txt" -type f | xargs cmake-format -i
