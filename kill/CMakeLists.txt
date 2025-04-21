cmake_minimum_required(VERSION 3.25)
project(testkill C)

set(CMAKE_C_STANDARD 11)


# Get the home directory
if(DEFINED ENV{HOME})
    set(HOME_DIR $ENV{HOME})
elseif(DEFINED ENV{USERPROFILE})  # For Windows compatibility
    set(HOME_DIR $ENV{USERPROFILE})
else()
    message(WARNING "Home directory not found, using project directory instead")
    set(HOME_DIR ${CMAKE_SOURCE_DIR})
endif()

# Print the value of HOME_DIR
message(STATUS "Home directory: ${HOME_DIR}")

# Define the output directory using the home directory
set(OUTPUT_DIR "${HOME_DIR}/lubeman")


# Set the output directory for executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})


# Add the directory containing libiniparser.a
set(LIBRARY_PATH ${CMAKE_SOURCE_DIR}/libs)
link_directories(${LIBRARY_PATH})
include_directories(${CMAKE_SOURCE_DIR}/libs/include)


add_executable(testkill
        asmjs.c
        tcpx.c
        main.c)

# Link the static library to your target
target_link_libraries(testkill PRIVATE ${LIBRARY_PATH}/libiniparser.a)







