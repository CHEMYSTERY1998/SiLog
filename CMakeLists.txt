cmake_minimum_required(VERSION 3.10)
project(silog_system)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含公共头文件目录
include_directories(${PROJECT_SOURCE_DIR}/interface)

# 添加编译选项
add_compile_options(-Wall -Wextra -Werror -DDEBUG)
# add_compile_options(-DSILOG_EXE)
add_compile_definitions(_GNU_SOURCE)

# 添加子目录
add_subdirectory(silog_comm)
add_subdirectory(silog_logger)
add_subdirectory(silog_dae)
add_subdirectory(silog_cat)
add_subdirectory(silog_exe)
add_subdirectory(test)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 格式化命令 find . -name "CMakeLists.txt" -type f | xargs cmake-format -i
