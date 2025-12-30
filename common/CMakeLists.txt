# 公共库，供其他模块依赖
add_library(common STATIC
    ${PROJECT_SOURCE_DIR}/common/src/silog_utils.c
    ${PROJECT_SOURCE_DIR}/common/src/silog_trans.c
    ${PROJECT_SOURCE_DIR}/common/src/silog_mpsc.c
)

target_include_directories(common PUBLIC
    ${PROJECT_SOURCE_DIR}/common/include
)
