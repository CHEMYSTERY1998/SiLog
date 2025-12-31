add_executable(silog ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_exe.c)

target_include_directories(silog PUBLIC ${PROJECT_SOURCE_DIR}/interface)

target_link_libraries(silog PRIVATE silog_common silog_dae silog_cat)
