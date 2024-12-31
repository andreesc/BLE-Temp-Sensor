# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/andre/esp/esp-idf/components/bootloader/subproject"
  "/home/andre/Documents/BLE-Temp-Sensor/Firmware/build/bootloader"
  "/home/andre/Documents/BLE-Temp-Sensor/Firmware/build/bootloader-prefix"
  "/home/andre/Documents/BLE-Temp-Sensor/Firmware/build/bootloader-prefix/tmp"
  "/home/andre/Documents/BLE-Temp-Sensor/Firmware/build/bootloader-prefix/src/bootloader-stamp"
  "/home/andre/Documents/BLE-Temp-Sensor/Firmware/build/bootloader-prefix/src"
  "/home/andre/Documents/BLE-Temp-Sensor/Firmware/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/andre/Documents/BLE-Temp-Sensor/Firmware/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/andre/Documents/BLE-Temp-Sensor/Firmware/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
