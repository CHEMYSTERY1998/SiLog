# 公共库，供其他模块依赖
add_library(
  silog_common STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_utils.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_trans.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_mpsc.c)

target_include_directories(silog_common PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_property(TARGET silog_common PROPERTY POSITION_INDEPENDENT_CODE ON)