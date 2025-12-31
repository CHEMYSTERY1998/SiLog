# 公共库，供其他模块依赖
add_library(
  common STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_utils.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_trans.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_mpsc.c)

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
