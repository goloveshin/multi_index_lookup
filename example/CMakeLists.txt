add_executable(
    multi_index_lookup_example
    main.cpp
)
target_link_libraries(
    multi_index_lookup_example
    PRIVATE
        multi_index_lookup::multi_index_lookup
)

add_test(
    NAME multi_index_lookup_example
    COMMAND multi_index_lookup_example
)
