add_executable(library_tests
    main.cpp
    LibraryServiceTests.cpp
)
target_link_libraries(library_tests PRIVATE library::core third_party_headers)
add_test(NAME library_tests COMMAND library_tests)
