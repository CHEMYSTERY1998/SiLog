# SiLog 测试模块
# ====================
# unit: 单元测试 - 测试单个函数/类的行为
# integration: 集成测试 - 测试多个模块协作的集成场景
# system: 系统/端到端测试 - 测试完整的系统行为
# fuzz: Fuzz/属性测试 - 使用模糊测试验证健壮性
# concurrency: 多线程/并发测试 - 测试并发场景下的线程安全性
# benchmark: 性能测试 - 测试系统性能指标
# reliability: 可靠性/稳定性测试 - 测试长时间运行的稳定性
# legacy: 保留原有的 C 语言测试（向后兼容）

# 添加新的测试子目录
add_subdirectory(unit)
add_subdirectory(integration)
# add_subdirectory(system)
# add_subdirectory(fuzz)
# add_subdirectory(concurrency)
# add_subdirectory(benchmark)
# add_subdirectory(reliability)
add_subdirectory(legacy)
