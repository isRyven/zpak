cmake_minimum_required(VERSION 3.12)

add_executable(test_zpak test_zpak.c)
target_link_libraries(test_zpak m zpak)
add_test(NAME test_zpak COMMAND test_zpak)

add_executable(benchmark_zpak bench_zpak.c)
target_link_libraries(benchmark_zpak m zpak)
