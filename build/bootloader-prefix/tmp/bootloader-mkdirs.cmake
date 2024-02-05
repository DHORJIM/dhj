# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/Usuario/esp/esp-idf/components/bootloader/subproject"
  "D:/Usuario/Documents/Empotrados/Proyecto_final/Proyecto/build/bootloader"
  "D:/Usuario/Documents/Empotrados/Proyecto_final/Proyecto/build/bootloader-prefix"
  "D:/Usuario/Documents/Empotrados/Proyecto_final/Proyecto/build/bootloader-prefix/tmp"
  "D:/Usuario/Documents/Empotrados/Proyecto_final/Proyecto/build/bootloader-prefix/src/bootloader-stamp"
  "D:/Usuario/Documents/Empotrados/Proyecto_final/Proyecto/build/bootloader-prefix/src"
  "D:/Usuario/Documents/Empotrados/Proyecto_final/Proyecto/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Usuario/Documents/Empotrados/Proyecto_final/Proyecto/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Usuario/Documents/Empotrados/Proyecto_final/Proyecto/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
