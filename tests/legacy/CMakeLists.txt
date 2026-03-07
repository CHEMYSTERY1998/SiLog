# SiLog 测试模块

# 保留原有的 C 语言测试（向后兼容）
set(TEST_EXECUTABLES silog_logger_test silog_ipc_test silog_time_test
                     silog_daemon_test silog_mpsc_test)

foreach(executable ${TEST_EXECUTABLES})
  add_executable(${executable} ${executable}.c)

  target_include_directories(
    ${executable}
    PRIVATE ${PROJECT_SOURCE_DIR}/interface
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_logger/include
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_dae/include
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_cat/include
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include)

  target_link_libraries(
    ${executable}
    PRIVATE silog_logger
    PRIVATE silog_dae
    PRIVATE silog_common)
endforeach()
