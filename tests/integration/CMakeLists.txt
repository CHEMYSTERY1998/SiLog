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

# TSan 模式下禁用测试发现（WSL2 兼容性问题）
if(ENABLE_TSAN)
    add_test(NAME silog_integrationtest COMMAND silog_integrationtest)
else()
    gtest_discover_tests(silog_integrationtest)
endif()
