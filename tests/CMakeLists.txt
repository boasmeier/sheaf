qt_add_executable(tst_database tst_database.cpp)
target_link_libraries(tst_database PRIVATE sheaf_core Qt6::Test)
sheaf_set_warnings(tst_database)
add_test(NAME tst_database COMMAND tst_database)
