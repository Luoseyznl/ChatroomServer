# 1. 添加测试可执行文件，包含测试代码和 utils 源文件
add_executable(test_utils
    test_utils.cpp
    ../src/utils/logger.cpp
    ../src/utils/timer.cpp
    # 如有其他 utils 源文件，继续添加
)

# 2. 包含头文件目录
target_include_directories(test_utils PRIVATE ../src)

# 3. 链接 GTest 和 pthread
target_link_libraries(test_utils
    GTest::gtest_main
    Threads::Threads
)

# 4. 添加测试
add_test(NAME test_utils COMMAND test_utils)