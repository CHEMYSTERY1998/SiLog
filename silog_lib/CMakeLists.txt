add_library(silog_lib STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_entry.c)

target_include_directories(
  silog_lib
  PUBLIC ${PROJECT_SOURCE_DIR}/interface
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(silog_lib PRIVATE common)
