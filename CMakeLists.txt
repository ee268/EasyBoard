cmake_minimum_required(VERSION 3.16)

project(EasyBoard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Core Gui Network Svg WebEngineWidgets LinguistTools)
find_package(Qt5 REQUIRED COMPONENTS Core)

file(GLOB_RECURSE EASYBOARD_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)
list(REMOVE_ITEM EASYBOARD_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/import/ebpdfrenderer.cpp")

add_executable(${PROJECT_NAME}
    ${EASYBOARD_SOURCES}
    resources/EasyBoard.qrc
)
configure_file(src/core/ebversion.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/ebversion.h" @ONLY)
set(EASYBOARD_EN_TS resources/translations/easyboard_en.ts)
set_source_files_properties(${EASYBOARD_EN_TS} PROPERTIES
    OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/translations")
qt5_add_translation(EASYBOARD_TRANSLATIONS ${EASYBOARD_EN_TS})
set(EASYBOARD_RUNTIME_DIR "${CMAKE_CURRENT_BINARY_DIR}")
if(CMAKE_CONFIGURATION_TYPES)
    set(EASYBOARD_RUNTIME_DIR "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")
endif()
add_custom_target(EasyBoardTranslations ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EASYBOARD_RUNTIME_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${EASYBOARD_TRANSLATIONS} ${EASYBOARD_RUNTIME_DIR}
    DEPENDS ${EASYBOARD_TRANSLATIONS}
    VERBATIM)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources")

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    enable_language(RC)
    configure_file(resources/easyboard.rc.in
        "${CMAKE_CURRENT_BINARY_DIR}/easyboard.rc" @ONLY)
    target_sources(${PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/easyboard.rc"
        resources/appIcon.rc)
    set_source_files_properties(resources/appIcon.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/easyboard.ico")
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()


target_link_libraries(EasyBoard PRIVATE Qt5::Core)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
    Qt5::Network
    Qt5::Svg
    Qt5::WebEngineWidgets
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.EasyBoard
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${EASYBOARD_TRANSLATIONS}
    DESTINATION ${CMAKE_INSTALL_BINDIR})

if(WIN32)
    add_executable(EasyBoardPdfRenderer src/import/ebpdfrenderer.cpp)
    target_compile_options(EasyBoardPdfRenderer PRIVATE /utf-8)
    target_link_libraries(EasyBoardPdfRenderer PRIVATE
        Qt5::Core Qt5::Gui windowsapp)
    install(TARGETS EasyBoardPdfRenderer
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
