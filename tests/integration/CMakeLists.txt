# SiLog 集成测试
# 测试多个模块协作的集成场景

enable_testing()

add_executable(silog_integrationtest
    main.cpp
    integration_test.cpp
)

target_include_directories(silog_integrationtest
    PRIVATE ${PROJECT_SOURCE_DIR}/interface
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include
)

target_link_libraries(silog_integrationtest
    PRIVATE silog_common
    PRIVATE gtest_main
)

include(GoogleTest)
gtest_discover_tests(silog_integrationtest)
