# SiLog 可靠性/稳定性测试
# 测试系统长时间运行的稳定性和可靠性

enable_testing()

add_executable(silog_reliabilitytest
    main.cpp
    reliability_test.cpp
)

target_include_directories(silog_reliabilitytest
    PRIVATE ${PROJECT_SOURCE_DIR}/interface
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include
)

target_link_libraries(silog_reliabilitytest
    PRIVATE silog_common
    PRIVATE GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(silog_reliabilitytest)
