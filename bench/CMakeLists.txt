INCLUDE_DIRECTORIES (
  ${CMAKE_SOURCE_DIR}/src
) 

set(bench_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/simplebench.cpp
)

add_executable(bench ${bench_SRCS})
target_link_libraries(bench
    pthread
)
