if(NOT CMAKE_BINARY_DIR STREQUAL "${CMAKE_SOURCE_DIR}")
  add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/hello_world
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}
  COMMAND ${CMAKE_COMMAND} -E copy
  ${CMAKE_CURRENT_SOURCE_DIR}/hello_world ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hello_world
  )
  add_custom_target(
  hello_world_copy ALL
  DEPENDS #hello_world
  ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/hello_world
  )
endif()
