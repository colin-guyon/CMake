project(foo)
add_executable(foo foo.cxx)
target_link_libraries(foo bar)
target_include_directories(foo 
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR})
