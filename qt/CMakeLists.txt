find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Svg)
qt_standard_project_setup()

# Qt < 6.9 still links the AGL framework on macOS, which newer SDKs no longer ship.
if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
    get_target_property(_gl_libs WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
    if(_gl_libs)
        list(FILTER _gl_libs EXCLUDE REGEX "AGL")
        set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES INTERFACE_LINK_LIBRARIES "${_gl_libs}")
    endif()
endif()

qt_add_executable(library_qt WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/LibraryController.cpp
    src/Theme.cpp
    src/Delegates.cpp
    src/Animations.cpp
    src/Painted.cpp
    src/Widgets.cpp
    src/Dialogs.cpp
    src/MainWindow.cpp
    src/pages/DashboardPage.cpp
    src/pages/BooksPage.cpp
    src/pages/StudentsPage.cpp
    src/pages/LoansPage.cpp
)
target_include_directories(library_qt PRIVATE src)
target_link_libraries(library_qt PRIVATE library::core Qt6::Widgets Qt6::Svg)

set_target_properties(library_qt PROPERTIES
    OUTPUT_NAME "LibraryManagement"
    MACOSX_BUNDLE_BUNDLE_NAME "书阁"
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.library.management"
)

# Icons are shared with the web front-end (assets/icons, Tabler Icons, MIT).
file(GLOB ICON_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/assets/icons/*.svg)
foreach(icon_file IN LISTS ICON_FILES)
    get_filename_component(icon_name ${icon_file} NAME)
    set_source_files_properties(${icon_file} PROPERTIES QT_RESOURCE_ALIAS ${icon_name})
endforeach()
qt_add_resources(library_qt "icons" PREFIX "/icons" FILES ${ICON_FILES})
qt_add_resources(library_qt "style" PREFIX "/style" BASE resources FILES resources/style.qss)
