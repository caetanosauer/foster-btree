INCLUDE_DIRECTORIES (
  ${CMAKE_SOURCE_DIR}/src
) 

ENABLE_TESTING()

INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/third-party/googletest/googletest/include)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

IF (COMPILER_IS_GCC)
    # strip out unused data/function. this also removes -rdynamic
    SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-fdata-sections -ffunction-sections")
    SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "-fdata-sections -ffunction-sections")
ENDIF (COMPILER_IS_GCC)

FUNCTION (X_ADD_TESTCASE targetname the_libraries)
    add_executable(${targetname} EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/${targetname}.cpp)
    target_link_libraries(${targetname} gtest ${the_libraries})
    # To pick up these xml files from Jenkins,
    # Set "**/test-reports/*.xml" as the report file filter.
    add_test(${targetname} ${targetname} --gtest_output=xml:test-reports/result_${targetname}.xml)
    add_dependencies(check ${targetname})
ENDFUNCTION()

X_ADD_TESTCASE(test_slotarray gtest)
X_ADD_TESTCASE(test_encoding gtest)
X_ADD_TESTCASE(test_node_simple gtest)
X_ADD_TESTCASE(test_node gtest)
X_ADD_TESTCASE(test_sorted_list gtest)
X_ADD_TESTCASE(test_btree gtest)
