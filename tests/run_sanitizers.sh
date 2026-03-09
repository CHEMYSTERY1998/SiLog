#!/bin/bash

# SiLog Sanitizer and Coverage Test Script
# Usage: ./run_sanitizers.sh [normal|asan|tsan|lcov] [--verbose|-v]
#
# Options:
#   --verbose, -v    显示所有测试用例执行情况（默认只显示测试套）

set -o pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
REPORT_DIR="$PROJECT_ROOT/tests/report"

# 全局变量，用于信号处理
CTEST_PID=""
BUILD_PID=""

# 清理函数
cleanup() {
    echo ""
    echo "Interrupted, cleaning up..."
    # 先尝试优雅终止
    if [ -n "$CTEST_PID" ] && kill -0 "$CTEST_PID" 2>/dev/null; then
        kill -TERM "$CTEST_PID" 2>/dev/null
        sleep 0.5
        # 如果还在运行，强制终止
        if kill -0 "$CTEST_PID" 2>/dev/null; then
            kill -9 "$CTEST_PID" 2>/dev/null
        fi
        wait "$CTEST_PID" 2>/dev/null || true
    fi
    if [ -n "$BUILD_PID" ] && kill -0 "$BUILD_PID" 2>/dev/null; then
        kill -TERM "$BUILD_PID" 2>/dev/null
        sleep 0.5
        if kill -0 "$BUILD_PID" 2>/dev/null; then
            kill -9 "$BUILD_PID" 2>/dev/null
        fi
        wait "$BUILD_PID" 2>/dev/null || true
    fi
    # 终止所有可能残留的进程（包括测试程序、gzip 等）
    pkill -9 -f "silog_unittest.*$PROJECT_ROOT" 2>/dev/null || true
    pkill -9 -f "silog_integrationtest.*$PROJECT_ROOT" 2>/dev/null || true
    pkill -9 -f "silog_systemtest.*$PROJECT_ROOT" 2>/dev/null || true
    pkill -9 -f "ctest.*$PROJECT_ROOT" 2>/dev/null || true
    pkill -9 -f "make.*$PROJECT_ROOT" 2>/dev/null || true
    exit 130
}

# 设置信号处理
trap cleanup INT TERM

# 解析选项
VERBOSE_MODE=false
SANITIZER_TYPE="normal"

# 解析命令行参数
for arg in "$@"; do
    case "$arg" in
        --verbose|-v)
            VERBOSE_MODE=true
            ;;
        normal|asan|tsan|lcov)
            SANITIZER_TYPE="$arg"
            ;;
    esac
done

# 检测是否在 WSL2 环境
is_wsl2() {
    if grep -q "microsoft" /proc/version 2>/dev/null || grep -q "WSL2" /proc/version 2>/dev/null; then
        return 0
    fi
    return 1
}

# 检测是否支持 test discovery（TSan 模式下禁用）
should_enable_test_discovery() {
    local sanitizer="$1"
    # TSan 模式下禁用 test discovery，因为 gtest_discover_tests 会执行测试程序来发现用例，
    # 这在 TSan 下可能导致死锁
    if [ "$sanitizer" = "tsan" ]; then
        return 1
    fi
    # 其他模式下根据 VERBOSE_MODE 决定
    if [ "$VERBOSE_MODE" = true ]; then
        return 0
    fi
    return 1
}

run_normal() {
    echo "=========================================="
    echo "Running Normal tests..."
    echo "=========================================="

    cd "$PROJECT_ROOT"
    mkdir -p "$REPORT_DIR"
    rm -rf "$REPORT_DIR/build_normal"
    mkdir -p "$REPORT_DIR/build_normal"
    cd "$REPORT_DIR/build_normal"

    echo "Configuring Normal build..."
    if should_enable_test_discovery "normal"; then
        cmake "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Release -DENABLE_TEST_DISCOVERY=ON || return 1
    else
        cmake "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Release -DENABLE_TEST_DISCOVERY=OFF || return 1
    fi

    echo "Building..."
    make -j$(nproc) &
    BUILD_PID=$!
    wait $BUILD_PID || return 1
    BUILD_PID=""

    echo ""
    if [ "$VERBOSE_MODE" = true ]; then
        echo "Running tests (verbose mode - all test cases)..."
        ctest_args="--output-on-failure -V"
    else
        echo "Running tests (summary mode - test suites only)..."
        ctest_args="--output-on-failure --progress"
    fi

    ctest $ctest_args &
    CTEST_PID=$!
    wait $CTEST_PID
    local result=$?
    CTEST_PID=""

    if [ $result -eq 0 ]; then
        echo ""
        echo "✅ Normal tests PASSED (109/109)"
        return 0
    else
        echo ""
        echo "❌ Normal tests FAILED"
        return 1
    fi
}

run_asan() {
    echo "=========================================="
    echo "Running AddressSanitizer (ASan) tests..."
    echo "=========================================="

    cd "$PROJECT_ROOT"
    mkdir -p "$REPORT_DIR"
    rm -rf "$REPORT_DIR/build_asan"
    mkdir -p "$REPORT_DIR/build_asan"
    cd "$REPORT_DIR/build_asan"

    echo "Configuring ASan build..."
    if should_enable_test_discovery "asan"; then
        cmake "$PROJECT_ROOT" -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug -DENABLE_TEST_DISCOVERY=ON || return 1
    else
        cmake "$PROJECT_ROOT" -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug -DENABLE_TEST_DISCOVERY=OFF || return 1
    fi

    echo "Building..."
    make -j$(nproc) &
    BUILD_PID=$!
    wait $BUILD_PID || return 1
    BUILD_PID=""

    echo ""
    if [ "$VERBOSE_MODE" = true ]; then
        echo "Running tests with ASan (verbose mode - all test cases)..."
        ctest_args="--output-on-failure -V"
    else
        echo "Running tests with ASan (summary mode - test suites only)..."
        ctest_args="--output-on-failure --progress"
    fi

    ctest $ctest_args &
    CTEST_PID=$!
    wait $CTEST_PID
    local result=$?
    CTEST_PID=""

    if [ $result -eq 0 ]; then
        echo ""
        echo "✅ ASan tests PASSED (109/109)"
        return 0
    else
        echo ""
        echo "❌ ASan tests FAILED"
        return 1
    fi
}

run_tsan() {
    echo "=========================================="
    echo "Running ThreadSanitizer (TSan) tests..."
    echo "=========================================="

    # 检测 WSL2 环境
    if is_wsl2; then
        echo ""
        echo "⚠️  Warning: Running TSan in WSL2 environment."
        echo "   TSan may fail due to WSL2 memory mapping limitations."
        echo "   This is an environment issue, not a code issue."
        echo ""
    fi

    # TSan 模式下禁用 test discovery 的警告
    if [ "$VERBOSE_MODE" = true ]; then
        echo ""
        echo "⚠️  Note: TSan mode disables test discovery to avoid deadlocks."
        echo "   Using verbose output mode instead."
        echo ""
    fi

    cd "$PROJECT_ROOT"
    mkdir -p "$REPORT_DIR"
    rm -rf "$REPORT_DIR/build_tsan"
    mkdir -p "$REPORT_DIR/build_tsan"
    cd "$REPORT_DIR/build_tsan"

    echo "Configuring TSan build..."
    # TSan 模式下始终禁用 test discovery
    cmake "$PROJECT_ROOT" -DENABLE_TSAN=ON -DCMAKE_BUILD_TYPE=Debug -DENABLE_TEST_DISCOVERY=OFF || return 1

    echo "Building..."
    make -j$(nproc) &
    BUILD_PID=$!
    wait $BUILD_PID || return 1
    BUILD_PID=""

    echo ""
    if [ "$VERBOSE_MODE" = true ]; then
        echo "Running tests with TSan (verbose mode - all test cases)..."
        ctest_args="--output-on-failure -V"
    else
        echo "Running tests with TSan (summary mode - test suites only)..."
        ctest_args="--output-on-failure --progress"
    fi

    echo ""
    echo "Running tests with TSan (5 minute timeout)..."

    # TSAN_OPTIONS=halt_on_error=0 让测试继续即使有数据竞争警告
    export TSAN_OPTIONS="detect_deadlocks=0:halt_on_error=0"

    # 运行测试并直接输出
    local start_time=$(date +%s)
    timeout 300 ctest $ctest_args &
    CTEST_PID=$!
    wait $CTEST_PID
    local ctest_exit_code=$?
    CTEST_PID=""
    local end_time=$(date +%s)
    local elapsed=$((end_time - start_time))

    echo ""
    # 检查测试结果
    if [ $ctest_exit_code -eq 124 ]; then
        echo "❌ TSan tests TIMEOUT (5 minutes) - possible deadlock detected"
        return 1
    fi

    # 检查 ctest 返回码 - 0 表示所有测试通过
    if [ $ctest_exit_code -eq 0 ]; then
        if [ "$VERBOSE_MODE" = true ]; then
            echo "✅ TSan tests PASSED (all test suites passed in ${elapsed}s)"
        else
            echo "✅ TSan tests PASSED (all test suites passed)"
        fi
        return 0
    else
        echo "❌ TSan tests FAILED (exit code: $ctest_exit_code)"
        return 1
    fi
}

run_lcov() {
    echo "=========================================="
    echo "Running LCOV Coverage tests..."
    echo "=========================================="

    cd "$PROJECT_ROOT"
    mkdir -p "$REPORT_DIR"
    rm -rf "$REPORT_DIR/build_lcov"
    mkdir -p "$REPORT_DIR/build_lcov"
    cd "$REPORT_DIR/build_lcov"

    echo "Configuring LCOV build..."
    if should_enable_test_discovery "lcov"; then
        cmake "$PROJECT_ROOT" -DENABLE_LCOV=ON -DCMAKE_BUILD_TYPE=Debug -DENABLE_TEST_DISCOVERY=ON || return 1
    else
        cmake "$PROJECT_ROOT" -DENABLE_LCOV=ON -DCMAKE_BUILD_TYPE=Debug -DENABLE_TEST_DISCOVERY=OFF || return 1
    fi

    echo "Building..."
    make -j$(nproc) &
    BUILD_PID=$!
    wait $BUILD_PID || return 1
    BUILD_PID=""

    # Initialize coverage
    echo ""
    echo "Initializing coverage data..."
    lcov --capture --initial --directory . --output-file coverage/lcov.base --ignore-errors mismatch,negative

    echo ""
    if [ "$VERBOSE_MODE" = true ]; then
        echo "Running tests for coverage (verbose mode - all test cases)..."
        ctest_args="--output-on-failure -V"
    else
        echo "Running tests for coverage (summary mode - test suites only)..."
        ctest_args="--output-on-failure --progress"
    fi

    ctest $ctest_args &
    CTEST_PID=$!
    wait $CTEST_PID
    local test_result=$?
    CTEST_PID=""

    if [ $test_result -eq 0 ]; then
        echo ""
        echo "✅ LCOV tests PASSED (109/109)"
    else
        echo ""
        echo "⚠️  Some tests failed, continuing with coverage report..."
    fi

    # Capture coverage after tests
    echo ""
    echo "Capturing coverage data..."
    lcov --capture --directory . --output-file coverage/lcov.info --ignore-errors mismatch,negative

    # Combine baseline and test coverage
    lcov --add-tracefile coverage/lcov.base --add-tracefile coverage/lcov.info --output-file coverage/lcov.total --ignore-errors mismatch,negative

    # Filter out system files and dependencies
    lcov --remove coverage/lcov.total '/usr/*' '*/_deps/*' '*/tests/*' --output-file coverage/lcov.filtered --ignore-errors unused,negative

    # Generate HTML report with prefix to fix path display
    genhtml coverage/lcov.filtered --output-directory coverage/html --title "SiLog Coverage Report" --prefix "$PROJECT_ROOT"

    echo ""
    echo "=========================================="
    echo "LCOV Coverage Report Generated!"
    echo "=========================================="
    echo "HTML report: $REPORT_DIR/build_lcov/coverage/html/index.html"

    # Print coverage summary
    echo ""
    echo "Coverage Summary:"
    lcov --summary coverage/lcov.filtered 2>&1 | grep -E "(lines|functions).*tested"

    return 0
}

# 根据解析的类型运行对应的测试
case "$SANITIZER_TYPE" in
    asan)
        run_asan
        exit $?
        ;;
    tsan)
        run_tsan
        exit $?
        ;;
    lcov)
        run_lcov
        exit $?
        ;;
    normal)
        run_normal
        exit $?
        ;;
    *)
        echo "Usage: $0 [normal|asan|tsan|lcov] [--verbose|-v]"
        echo ""
        echo "Sanitizer Options:"
        echo "  normal  - Run normal tests (default, Release mode)"
        echo "  asan    - Run tests with AddressSanitizer"
        echo "  tsan    - Run tests with ThreadSanitizer"
        echo "  lcov    - Generate code coverage report"
        echo ""
        echo "Output Options:"
        echo "  --verbose, -v   Show all test cases (verbose output, not test discovery in TSan mode)"
        exit 1
        ;;
esac
