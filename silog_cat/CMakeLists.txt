add_library(silog_cat SHARED ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_cat.c)

target_include_directories(silog_cat
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_include_directories(
  silog_cat
  PUBLIC ${PROJECT_SOURCE_DIR}/interface
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include)

target_link_libraries(silog_cat PRIVATE silog_common)
