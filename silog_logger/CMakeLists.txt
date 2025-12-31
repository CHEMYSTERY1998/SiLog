add_library(silog_logger SHARED ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_logger.c)

target_include_directories(
  silog_logger
  PUBLIC ${PROJECT_SOURCE_DIR}/silog_comm/include/
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/silog_common/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(silog_logger PRIVATE silog_common)
