cmake_minimum_required(VERSION 4.0)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(multi_index_lookup LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED YES)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(MULTI_INDEX_LOOKUP_IS_TOP_LEVEL ON)
else()
    set(MULTI_INDEX_LOOKUP_IS_TOP_LEVEL OFF)
endif()

# the tests are built by default only when multi_index_lookup is the top-level project
option(MULTI_INDEX_LOOKUP_BUILD_TESTS "Build multi_index_lookup tests" ${MULTI_INDEX_LOOKUP_IS_TOP_LEVEL})

add_library(multi_index_lookup INTERFACE)
add_library(multi_index_lookup::multi_index_lookup ALIAS multi_index_lookup)

target_include_directories(
    multi_index_lookup
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(
    multi_index_lookup
    INTERFACE
        cxx_std_23
)

if(NOT TARGET Boost::multi_index)
    include(FetchContent)
    set(BOOST_INCLUDE_LIBRARIES multi_index)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    # the release archive, so configure does not clone every Boost submodule
    FetchContent_Declare(
        Boost
        URL https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-cmake.tar.xz
        EXCLUDE_FROM_ALL
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(Boost)
endif()

target_link_libraries(
    multi_index_lookup
    INTERFACE
        Boost::multi_index
)

if(MULTI_INDEX_LOOKUP_BUILD_TESTS)
    if(NOT TARGET doctest::doctest)
        include(FetchContent)
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
        FetchContent_Declare(
            doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG v2.5.0
            EXCLUDE_FROM_ALL
        )
        FetchContent_MakeAvailable(doctest)
    endif()

    add_executable(
        multi_index_lookup_tests
        tests/test_multi_index_lookup.cpp
    )
    target_link_libraries(
        multi_index_lookup_tests
        PRIVATE
            multi_index_lookup::multi_index_lookup
            doctest::doctest
    )
    target_compile_definitions(
        multi_index_lookup_tests
        PRIVATE
            DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
    )

    enable_testing()
    add_test(
        NAME multi_index_lookup_tests
        COMMAND multi_index_lookup_tests
    )

    add_subdirectory(example)
endif()
