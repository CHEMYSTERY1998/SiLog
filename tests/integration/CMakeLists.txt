# SiLog 集成测试
# 测试多个模块协作的集成场景

enable_testing()

add_executable(silog_integrationtest
    main.cpp
    silog_logger_print_test.cpp
    prelog_integration_test.cpp
    remote_integration_test.cpp
)

target_include_directories(silog_integrationtest
    PRIVATE ${PROJECT_SOURCE_DIR}/interface
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_logger/include
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_dae/include
)

target_link_libraries(silog_integrationtest
    PRIVATE silog_common
    PRIVATE silog_logger
    PRIVATE silog_dae
    PRIVATE gtest_main
    PRIVATE pthread
)

include(GoogleTest)

# 根据 ENABLE_TEST_DISCOVERY 选项决定测试发现方式
# ON:  使用 gtest_discover_tests 显示单个测试用例（verbose 模式）
# OFF: 使用 add_test 只显示测试套（summary 模式）
if(ENABLE_TEST_DISCOVERY)
    message(STATUS "Test discovery enabled - individual test cases will be shown")
    gtest_discover_tests(silog_integrationtest DISCOVERY_MODE PRE_TEST)
else()
    message(STATUS "Test discovery disabled - only test suites will be shown")
    add_test(NAME silog_integrationtest COMMAND silog_integrationtest)
endif()
