INCLUDE_DIRECTORIES (
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/third-party/spdlog/include
) 


add_executable(simplebench ${CMAKE_CURRENT_SOURCE_DIR}/simplebench.cpp)
target_link_libraries(simplebench pthread)

add_executable(latchtest ${CMAKE_CURRENT_SOURCE_DIR}/latchtest.cpp)
target_link_libraries(latchtest pthread)
