cmake_minimum_required(VERSION 3.14.1)
project(si_log)
set(SI_LOG_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "-g")

# 设置源码目录
set(SI_LOG_SOURCE ${SI_LOG_ROOT}/src)

# 设置源码文件
message(STATUS "[*] searching *.cpp")
foreach(file_path ${SI_LOG_SOURCE})
  file(GLOB file_source_cpp ${file_path}/*.cpp)
  list(APPEND SI_LOG.SourceCPP ${file_source_cpp})
endforeach()

message(STATUS "[*] searching *.c")
foreach(file_path ${SI_LOG_SOURCE})
  file(GLOB file_source_c ${file_path}/*.c)
  list(APPEND SI_LOG.SourceC ${file_source_c})
endforeach()

# 设置头文件
include_directories(${SI_LOG_ROOT}/include)

# add_executable(simpo_plus ${SI_LOG.SourceCPP} ${SI_LOG.SourceC})

# 编译动态库
add_library(si_log_shared SHARED ${SI_LOG.SourceCPP} ${SI_LOG.SourceC})
set_target_properties(si_log_shared PROPERTIES OUTPUT_NAME "si_log")

message("all:" ${SI_LOG.SourceC} ${SI_LOG.SourceCPP})

