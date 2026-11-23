# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/yetif/Documents/Coding/C++/Rocket/project/ground/app")
  file(MAKE_DIRECTORY "C:/Users/yetif/Documents/Coding/C++/Rocket/project/ground/app")
endif()
file(MAKE_DIRECTORY
  "C:/Users/yetif/Documents/Coding/C++/Rocket/build/project/ground-app-build"
  "C:/Users/yetif/Documents/Coding/C++/Rocket/build/project/ground_app-prefix"
  "C:/Users/yetif/Documents/Coding/C++/Rocket/build/project/ground_app-prefix/tmp"
  "C:/Users/yetif/Documents/Coding/C++/Rocket/build/project/ground_app-prefix/src/ground_app-stamp"
  "C:/Users/yetif/Documents/Coding/C++/Rocket/build/project/ground_app-prefix/src"
  "C:/Users/yetif/Documents/Coding/C++/Rocket/build/project/ground_app-prefix/src/ground_app-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/yetif/Documents/Coding/C++/Rocket/build/project/ground_app-prefix/src/ground_app-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/yetif/Documents/Coding/C++/Rocket/build/project/ground_app-prefix/src/ground_app-stamp${cfgdir}") # cfgdir has leading slash
endif()
