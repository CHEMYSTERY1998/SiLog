# SiLog 系统/端到端测试
# 测试完整的系统行为和端到端场景

enable_testing()

add_executable(silog_systemtest
    main.cpp
    system_test.cpp
)

target_include_directories(silog_systemtest
    PRIVATE ${PROJECT_SOURCE_DIR}/interface
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include
)

target_link_libraries(silog_systemtest
    PRIVATE silog_common
    PRIVATE gtest_main
)

include(GoogleTest)
gtest_discover_tests(silog_systemtest)
