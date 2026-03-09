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
    utils_test.cpp
    file_manager_test.cpp
    daemon_test.cpp
    file_manager_exception_test.cpp
    daemon_exception_test.cpp
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
    PRIVATE GTest::gtest_main
)

include(GoogleTest)

# 根据 ENABLE_TEST_DISCOVERY 选项决定测试发现方式
# ON:  使用 gtest_discover_tests 显示单个测试用例（verbose 模式）
# OFF: 使用 add_test 只显示测试套（summary 模式）
if(ENABLE_TEST_DISCOVERY)
    message(STATUS "Test discovery enabled - individual test cases will be shown")
    gtest_discover_tests(silog_unittest DISCOVERY_MODE PRE_TEST)
else()
    message(STATUS "Test discovery disabled - only test suites will be shown")
    add_test(NAME silog_unittest COMMAND silog_unittest)
endif()
