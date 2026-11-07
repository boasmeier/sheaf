cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
project(sheaf VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick Pdf PdfQuick Sql Test)
qt_standard_project_setup(REQUIRES 6.4)
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)
set(QT_SKIP_AUTO_QML_PLUGIN_INCLUSION ON)

# Warnings as errors in debug builds.
function(sheaf_set_warnings target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic
                                             $<$<CONFIG:Debug>:-Werror>)
  endif()
endfunction()

# Backend library, shared by the app and the tests.
add_library(sheaf_core STATIC
  src/storage/Database.cpp
  src/storage/Database.h
  src/storage/PositionStore.cpp
  src/storage/PositionStore.h
  src/util/FileIdentity.cpp
  src/util/FileIdentity.h
  src/app/NavigationHistory.cpp
  src/app/NavigationHistory.h
  src/app/SessionModel.cpp
  src/app/SessionModel.h
  src/app/TabController.cpp
  src/app/TabController.h
)
target_include_directories(sheaf_core PUBLIC src)
target_link_libraries(sheaf_core PUBLIC Qt6::Core Qt6::Sql)
sheaf_set_warnings(sheaf_core)

qt_add_executable(sheaf src/main.cpp)
qt_add_qml_module(sheaf
  URI Sheaf
  VERSION 1.0
  QML_FILES qml/Main.qml qml/Viewer.qml qml/PdfPane.qml qml/SheafPdfView.qml qml/Sidebar.qml
)
target_link_libraries(sheaf PRIVATE sheaf_core Qt6::Quick Qt6::Pdf Qt6::PdfQuick)
sheaf_set_warnings(sheaf)

enable_testing()
add_subdirectory(tests)
