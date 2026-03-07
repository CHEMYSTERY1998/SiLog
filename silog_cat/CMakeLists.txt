add_executable(silogcat ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_cat.c)

target_include_directories(
  silogcat
  PUBLIC ${PROJECT_SOURCE_DIR}/interface
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include
  PRIVATE ${PROJECT_SOURCE_DIR}/silog_logger/include)

target_link_libraries(silogcat PRIVATE silog_common PRIVATE silog_logger)
