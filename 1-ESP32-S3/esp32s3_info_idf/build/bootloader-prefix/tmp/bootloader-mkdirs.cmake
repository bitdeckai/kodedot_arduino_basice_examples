# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "H:/esp32/Espressif/Espressif/frameworks/esp-idf-v5.5/components/bootloader/subproject")
  file(MAKE_DIRECTORY "H:/esp32/Espressif/Espressif/frameworks/esp-idf-v5.5/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
  "E:/0_project/Kode/kode_download/Examples/1-ESP32-S3/esp32s3_info_idf/build/bootloader"
  "E:/0_project/Kode/kode_download/Examples/1-ESP32-S3/esp32s3_info_idf/build/bootloader-prefix"
  "E:/0_project/Kode/kode_download/Examples/1-ESP32-S3/esp32s3_info_idf/build/bootloader-prefix/tmp"
  "E:/0_project/Kode/kode_download/Examples/1-ESP32-S3/esp32s3_info_idf/build/bootloader-prefix/src/bootloader-stamp"
  "E:/0_project/Kode/kode_download/Examples/1-ESP32-S3/esp32s3_info_idf/build/bootloader-prefix/src"
  "E:/0_project/Kode/kode_download/Examples/1-ESP32-S3/esp32s3_info_idf/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/0_project/Kode/kode_download/Examples/1-ESP32-S3/esp32s3_info_idf/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/0_project/Kode/kode_download/Examples/1-ESP32-S3/esp32s3_info_idf/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
