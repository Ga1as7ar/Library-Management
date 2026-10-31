add_library(library_core STATIC
    src/Database.cpp
    src/LibraryService.cpp
    src/Models.cpp
)
add_library(library::core ALIAS library_core)

target_include_directories(library_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(library_core PUBLIC SQLite3::SQLite3)
