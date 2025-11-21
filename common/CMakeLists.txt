# 公共库，供其他模块依赖
add_library(common STATIC
    src/common_utils.cpp
    src/common_log.cpp
)

target_include_directories(common PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/common
)
