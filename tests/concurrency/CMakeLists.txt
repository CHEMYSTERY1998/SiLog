# SiLog 多线程/并发测试
# 测试并发场景下的线程安全性和正确性

enable_testing()

add_executable(silog_concurrencytest
    main.cpp
    concurrency_test.cpp
)

target_include_directories(silog_concurrencytest
    PRIVATE ${PROJECT_SOURCE_DIR}/interface
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include
)

target_link_libraries(silog_concurrencytest
    PRIVATE silog_common
    PRIVATE GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(silog_concurrencytest)
