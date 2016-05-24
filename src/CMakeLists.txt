INCLUDE_DIRECTORIES (
  ${CMAKE_SOURCE_DIR}/src
)

set(foster_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

add_library(foster STATIC ${foster_SRCS})
# target_link_libraries(sm
#     lib1
#     lib2
#     )
