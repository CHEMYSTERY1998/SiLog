# 定义可执行文件列表
set(TEST_EXECUTABLES silog_entry_test silog_trans_test)

# 创建可执行文件
foreach(executable ${TEST_EXECUTABLES})
  add_executable(${executable} ${executable}.c)
  # 包含头文件目录
  target_include_directories(
    ${executable}
    PRIVATE ${PROJECT_SOURCE_DIR}/interface
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_lib/include)
  # 链接公共库
  target_link_libraries(
    ${executable}
    PRIVATE silog_lib
    PRIVATE common)
endforeach()
