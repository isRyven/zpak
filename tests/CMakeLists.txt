add_executable(test_zpak test_zpak.c)
target_include_directories(test_zpak PRIVATE ${CMAKE_SOURCE_DIR}/deps)
target_link_libraries(test_zpak m zpak)
add_test(NAME test_zpak COMMAND test_zpak)

add_executable(benchmark_zpak bench_zpak.c)
target_include_directories(benchmark_zpak PRIVATE ${CMAKE_SOURCE_DIR}/deps)
target_link_libraries(benchmark_zpak m zpak)
