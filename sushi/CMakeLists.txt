# CMakeList.txt : CMake project for sushi-cmake, include source and define
# project specific logic here.


# Add source to this project's executable.
add_executable (sushi "sushi.cpp" "sushi.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET sushi PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
