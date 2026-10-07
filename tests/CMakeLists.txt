add_executable(rapid_stats_tests
    main.cpp
    order_statistic_test.cpp
)
target_link_libraries(rapid_stats_tests PRIVATE rapid_stats)
rapid_stats_configure_target(rapid_stats_tests)
rapid_stats_set_output_directory(rapid_stats_tests)

add_test(NAME order_statistic COMMAND rapid_stats_tests)
set_tests_properties(order_statistic PROPERTIES LABELS "unit")
