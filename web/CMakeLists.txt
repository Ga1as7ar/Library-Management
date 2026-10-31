find_package(Threads REQUIRED)

add_executable(library_web
    server/main.cpp
    server/ApiRoutes.cpp
    server/JsonMapping.cpp
)
target_link_libraries(library_web PRIVATE library::core third_party_headers Threads::Threads)
target_compile_definitions(library_web PRIVATE
    LIBRARY_WEB_PUBLIC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/public"
)
