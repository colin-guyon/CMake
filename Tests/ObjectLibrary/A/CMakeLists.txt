project(ObjectLibraryA C)
# Add -fPIC so objects can be used in shared libraries.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_definitions(-DA_DEF)

add_custom_command(
  OUTPUT ${CMAKE_CFG_INTDIR}/a1.c
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/a1.c.in
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/a1.c.in
                                   ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/a1.c
  )

# Remove the custom command output to be sure it runs in an
# incremental test.
file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/a.cmake)
add_custom_command(
    OUTPUT ${CMAKE_CFG_INTDIR}/a.cmake
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/a.cmake
  )

add_library(A OBJECT ${CMAKE_CFG_INTDIR}/a1.c a2.c ${CMAKE_CFG_INTDIR}/a.cmake)
target_include_directories(A PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET A PROPERTY COMPILE_PDB_NAME Apdb)
