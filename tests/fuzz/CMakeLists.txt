# SiLog Fuzz/属性测试
# 使用模糊测试和属性测试验证系统健壮性

enable_testing()

add_executable(silog_fuzz
    main.cpp
    queue_fuzz.cpp
)

target_include_directories(silog_fuzz
    PRIVATE ${PROJECT_SOURCE_DIR}/interface
    PRIVATE ${PROJECT_SOURCE_DIR}/silog_comm/include
)

target_link_libraries(silog_fuzz
    PRIVATE silog_common
    PRIVATE gtest
)

include(GoogleTest)
gtest_discover_tests(silog_fuzz)
