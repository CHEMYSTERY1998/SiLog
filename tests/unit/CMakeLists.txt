# SiLog 单元测试
# 测试单个函数/类的行为

enable_testing()

add_executable(silog_unittest
    main.cpp
    mpsc_test.cpp
    pqueue_test.cpp
    time_test.cpp
    securec_test.cpp
    ipc_test.cpp
    prelog_test.cpp
    remote_test.cpp
)

target_include_directories(silog_unittest
    PRIVATE ${PROJECT_SOURCE_DIR}/interface
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_dae/include
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_logger/include
)

target_link_libraries(silog_unittest
    PRIVATE silog_common
    PRIVATE silog_dae
    PRIVATE gtest_main
)

include(GoogleTest)

# TSan 模式下禁用测试发现（WSL2 兼容性问题）
if(ENABLE_TSAN)
    # 使用简单的测试添加方式
    add_test(NAME silog_unittest COMMAND silog_unittest)
else()
    gtest_discover_tests(silog_unittest)
endif()
