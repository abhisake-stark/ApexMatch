# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release/_deps/usockets-src")
  file(MAKE_DIRECTORY "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release/_deps/usockets-src")
endif()
file(MAKE_DIRECTORY
  "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release/_deps/usockets-build"
  "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release/_deps/usockets-subbuild/usockets-populate-prefix"
  "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release/_deps/usockets-subbuild/usockets-populate-prefix/tmp"
  "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release/_deps/usockets-subbuild/usockets-populate-prefix/src/usockets-populate-stamp"
  "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release/_deps/usockets-subbuild/usockets-populate-prefix/src"
  "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release/_deps/usockets-subbuild/usockets-populate-prefix/src/usockets-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release/_deps/usockets-subbuild/usockets-populate-prefix/src/usockets-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release/_deps/usockets-subbuild/usockets-populate-prefix/src/usockets-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
