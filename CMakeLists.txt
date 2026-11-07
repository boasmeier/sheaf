# Copyright (C) 2026 Boas Meier
# SPDX-License-Identifier: GPL-3.0-or-later

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
  src/storage/SessionStore.cpp
  src/storage/SessionStore.h
  src/storage/PositionStore.h
  src/util/FileIdentity.cpp
  src/util/FileIdentity.h
  src/app/NavigationHistory.cpp
  src/app/NavigationHistory.h
  src/app/SessionModel.cpp
  src/app/SessionModel.h
  src/app/TabController.cpp
  src/app/QmlTypes.cpp
  src/app/QmlTypes.h
  src/search/PdfSearchSession.cpp
  src/search/PdfSearchSession.h
  src/search/SearchResults.cpp
  src/search/SearchResults.h
  src/search/TextMatcher.cpp
  src/search/TextMatcher.h
  src/app/UnixSignals.cpp
  src/app/UnixSignals.h
  src/app/TabController.h
)
target_include_directories(sheaf_core PUBLIC src)
target_link_libraries(sheaf_core PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Pdf Qt6::Sql)
sheaf_set_warnings(sheaf_core)

qt_add_executable(sheaf src/main.cpp)
qt_add_qml_module(sheaf
  URI Sheaf
  VERSION 1.0
  QML_FILES qml/Main.qml qml/Viewer.qml qml/PdfPane.qml qml/SheafPdfView.qml qml/Sidebar.qml qml/SearchBar.qml
)
target_link_libraries(sheaf PRIVATE sheaf_core Qt6::Quick Qt6::Pdf Qt6::PdfQuick)
sheaf_set_warnings(sheaf)

# --- Installation ---
# cmake --install build [--prefix ~/.local]   (per user, no root needed)
include(GNUInstallDirs)
install(TARGETS sheaf RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/sheaf.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
# The launcher (what GNOME shows in the app grid and can pin to the dock) must name the program by its absolute path,
# because a desktop session does not necessarily have ~/.local/bin in its PATH. The prefix can be chosen at install time
# (--prefix, DESTDIR), so the file is written then, not when configuring. (file(INSTALL) applies DESTDIR by itself.)
install(CODE "
  set(SHEAF_EXEC \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/sheaf\")
  if(SHEAF_EXEC MATCHES \" \")
    set(SHEAF_EXEC \"\\\"\${SHEAF_EXEC}\\\"\")  # the desktop entry spec wants a path with spaces in quotes
  endif()
  configure_file(\"${CMAKE_CURRENT_SOURCE_DIR}/data/sheaf.desktop.in\" \"${CMAKE_CURRENT_BINARY_DIR}/sheaf.desktop\" @ONLY)
  file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/applications\"
       TYPE FILE FILES \"${CMAKE_CURRENT_BINARY_DIR}/sheaf.desktop\")
")

# Desktops read icons and file associations through caches. A stale icon cache (an earlier app that installed into the
# same ~/.local/share/icons/hicolor may have left one) hides a new icon: GNOME then shows the placeholder. So when
# installing into a real prefix, refresh the caches, as an app's installer normally does. Packagers (DESTDIR) refresh
# them in their own post-install step.
find_program(SHEAF_GTK_UPDATE_ICON_CACHE gtk-update-icon-cache)
find_program(SHEAF_UPDATE_DESKTOP_DATABASE update-desktop-database)
if(SHEAF_GTK_UPDATE_ICON_CACHE)
  install(CODE "
    if(\"\$ENV{DESTDIR}\" STREQUAL \"\")
      execute_process(COMMAND \"${SHEAF_GTK_UPDATE_ICON_CACHE}\" -f -t
                              \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/icons/hicolor\"
                      RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
      if(result EQUAL 0)
        message(STATUS \"Refreshed the icon cache\")
      endif()
    endif()
  ")
endif()
if(SHEAF_UPDATE_DESKTOP_DATABASE)
  install(CODE "
    if(\"\$ENV{DESTDIR}\" STREQUAL \"\")
      execute_process(COMMAND \"${SHEAF_UPDATE_DESKTOP_DATABASE}\" \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/applications\"
                      RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
      if(result EQUAL 0)
        message(STATUS \"Refreshed the desktop database\")
      endif()
    endif()
  ")
endif()

enable_testing()
add_subdirectory(tests)
