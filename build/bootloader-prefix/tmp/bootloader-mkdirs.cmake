# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/OWNER/esp/esp-idf/components/bootloader/subproject"
  "C:/Users/OWNER/Desktop/ESP_IDF_PROJECTS/recorder_2/build/bootloader"
  "C:/Users/OWNER/Desktop/ESP_IDF_PROJECTS/recorder_2/build/bootloader-prefix"
  "C:/Users/OWNER/Desktop/ESP_IDF_PROJECTS/recorder_2/build/bootloader-prefix/tmp"
  "C:/Users/OWNER/Desktop/ESP_IDF_PROJECTS/recorder_2/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/OWNER/Desktop/ESP_IDF_PROJECTS/recorder_2/build/bootloader-prefix/src"
  "C:/Users/OWNER/Desktop/ESP_IDF_PROJECTS/recorder_2/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/OWNER/Desktop/ESP_IDF_PROJECTS/recorder_2/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
