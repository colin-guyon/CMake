if(NOT LIBRARY_OUTPUT_PATH)
    if(CMAKE_CONFIGURATION_TYPES)
        link_directories(${Jump_BINARY_DIR}/Library/Static/${CMAKE_CFG_INTDIR}
                    ${Jump_BINARY_DIR}/Library/Shared/${CMAKE_CFG_INTDIR})
    else()
        link_directories(${Jump_BINARY_DIR}/Library/Static
                    ${Jump_BINARY_DIR}/Library/Shared)
    endif()
endif()
add_executable(jumpExecutable jumpExecutable.cxx)
target_link_libraries(jumpExecutable jumpStatic jumpShared)
