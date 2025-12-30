cmake_minimum_required(VERSION 3.10)
project(silog_system)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含公共头文件目录
include_directories(
    ${PROJECT_SOURCE_DIR}/interface
)

# 添加编译选项
add_compile_options(-Wall -Wextra -Werror -DDEBUG)

# 添加子目录
add_subdirectory(common)
add_subdirectory(siloglib)
# add_subdirectory(silogcat)
# add_subdirectory(silogd)

add_subdirectory(test)
