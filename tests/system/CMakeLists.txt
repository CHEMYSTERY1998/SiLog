# SiLog 系统/端到端测试
# 测试完整的系统行为和端到端场景

enable_testing()

add_executable(silog_systemtest
    main.cpp
    system_test.cpp
    daemon_system_test.cpp
)

target_include_directories(silog_systemtest
    PRIVATE ${PROJECT_SOURCE_DIR}/interface
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_dae/include
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_logger/include
)

target_link_libraries(silog_systemtest
    PRIVATE silog_common
    PRIVATE silog_dae
    PRIVATE silog_logger
    PRIVATE gtest_main
    PRIVATE pthread
)

include(GoogleTest)

# 根据 ENABLE_TEST_DISCOVERY 选项决定测试发现方式
if(ENABLE_TEST_DISCOVERY)
    message(STATUS "Test discovery enabled - individual test cases will be shown")
    gtest_discover_tests(silog_systemtest DISCOVERY_MODE PRE_TEST)
else()
    message(STATUS "Test discovery disabled - only test suites will be shown")
    add_test(NAME silog_systemtest COMMAND silog_systemtest)
endif()
