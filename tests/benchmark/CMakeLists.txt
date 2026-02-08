# SiLog 性能测试
# 测试系统性能指标和基准测试

enable_testing()

add_executable(silog_benchmark
    main.cpp
    benchmark_test.cpp
)

target_include_directories(silog_benchmark
    PRIVATE ${PROJECT_SOURCE_DIR}/interface
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include
)

target_link_libraries(silog_benchmark
    PRIVATE silog_common
    PRIVATE gtest_main
)

include(GoogleTest)
gtest_discover_tests(silog_benchmark)
