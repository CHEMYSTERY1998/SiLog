# 公共库，供其他模块依赖
add_library(
  silog_common STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_utils.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_ipc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_time.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_mpsc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_pqueue.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_securec.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_prelog.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_remote.c)

target_include_directories(silog_common
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
                           PRIVATE ${PROJECT_SOURCE_DIR}/silog_logger/include)

set_property(TARGET silog_common PROPERTY POSITION_INDEPENDENT_CODE ON)
