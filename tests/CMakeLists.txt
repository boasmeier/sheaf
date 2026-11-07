foreach(name database fileidentity positionstore sessionstore navigationhistory sessionmodel tabcontroller)
  qt_add_executable(tst_${name} tst_${name}.cpp)
  target_link_libraries(tst_${name} PRIVATE sheaf_core Qt6::Test)
  sheaf_set_warnings(tst_${name})
  add_test(NAME tst_${name} COMMAND tst_${name})
endforeach()

# QML tests: viewer behaviour, position restore and tab switching, run against the real QML sources.
find_package(Qt6 REQUIRED COMPONENTS QuickTest Gui)
qt_add_executable(tst_qml qml_main.cpp)
target_link_libraries(tst_qml PRIVATE sheaf_core Qt6::QuickTest Qt6::Quick Qt6::Gui Qt6::Pdf Qt6::PdfQuick)
target_compile_definitions(tst_qml PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
sheaf_set_warnings(tst_qml)
add_test(NAME tst_qml COMMAND tst_qml)
# Software rendering keeps pixel comparisons deterministic and lets the tests run headless.
set_tests_properties(tst_qml PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
  TIMEOUT 300)

# Starts the real application and quits it with signals, like a logout does.
qt_add_executable(tst_quit tst_quit.cpp)
target_link_libraries(tst_quit PRIVATE sheaf_core Qt6::Test Qt6::Gui)
target_compile_definitions(tst_quit PRIVATE SHEAF_BINARY="$<TARGET_FILE:sheaf>")
add_dependencies(tst_quit sheaf)
sheaf_set_warnings(tst_quit)
add_test(NAME tst_quit COMMAND tst_quit)
set_tests_properties(tst_quit PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
