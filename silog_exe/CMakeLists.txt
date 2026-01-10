add_executable(silog ${CMAKE_CURRENT_SOURCE_DIR}/src/silog_exe.c)

target_include_directories(silog
    PRIVATE ${PROJECT_SOURCE_DIR}/interface
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_dae/include
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include
)

target_link_libraries(silog PRIVATE silog_common silog_dae silog_cat)
