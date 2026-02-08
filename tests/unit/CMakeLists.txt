# SiLog 单元测试
# 测试单个函数/类的行为

enable_testing()

add_executable(silog_unittest
    main.cpp
    mpsc_test.cpp
    pqueue_test.cpp
    time_test.cpp
)

target_include_directories(silog_unittest
    PRIVATE ${PROJECT_SOURCE_DIR}/interface
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include
)

target_link_libraries(silog_unittest
    PRIVATE silog_common
    PRIVATE gtest_main
)

include(GoogleTest)
gtest_discover_tests(silog_unittest)
