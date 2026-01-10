# 定义可执行文件列表
set(TEST_EXECUTABLES silog_logger_test silog_trans_test silog_time_test)

# 创建可执行文件
foreach(executable ${TEST_EXECUTABLES})
  add_executable(${executable} ${executable}.c)
  # 包含头文件目录
  target_include_directories(
    ${executable}
    PRIVATE ${PROJECT_SOURCE_DIR}/interface
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_logger/include
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_dae/include
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_cat/include
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include
    )
  # 链接公共库
  target_link_libraries(
    ${executable}
    PRIVATE silog_logger
    PRIVATE silog_dae
    PRIVATE silog_cat
    PRIVATE silog_common)
endforeach()
