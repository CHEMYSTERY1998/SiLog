# 日志库，对外提供 API
add_library(siloglib STATIC ${PROJECT_SOURCE_DIR}/silogd/src/silog_api.cpp)

target_link_libraries(siloglib PUBLIC common)

target_include_directories(siloglib PUBLIC ${PROJECT_SOURCE_DIR}/silogd/include
                                           ${PROJECT_SOURCE_DIR}/interface)
