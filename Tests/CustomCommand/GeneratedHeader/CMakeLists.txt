# Simulate in-source build include-file behavior for out-of-source
# builds.
set(CMAKE_INCLUDE_CURRENT_DIR 1)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/generated.h
  COMMAND
  ${CMAKE_COMMAND} ARGS -E
  copy ${CMAKE_CURRENT_SOURCE_DIR}/generated.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/generated.h
)

include_directories("${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}")

add_library(GeneratedHeader main.cpp ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/generated.h)

