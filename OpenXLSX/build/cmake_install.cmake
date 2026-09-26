# Install script for directory: /mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/OpenXLSX/headers" TYPE FILE FILES "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/build/OpenXLSX-Exports.hpp")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/OpenXLSX/headers" TYPE FILE FILES
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/IZipArchive.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLCell.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLCellIterator.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLCellRange.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLCellReference.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLCellValue.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLColor.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLColumn.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLCommandQuery.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLComments.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLConstants.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLContentTypes.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLDateTime.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLDocument.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLDrawing.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLException.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLFormula.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLIterator.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLMergeCells.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLProperties.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLRelationships.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLRow.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLRowData.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLSharedStrings.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLSheet.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLStyles.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLTables.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLWorkbook.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLXmlData.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLXmlFile.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLXmlParser.hpp"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/headers/XLZipArchive.hpp"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/OpenXLSX" TYPE FILE FILES "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/OpenXLSX.hpp")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xlibx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/build/libOpenXLSX.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/OpenXLSX" TYPE FILE FILES
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/OpenXLSXConfig.cmake"
    "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/build/OpenXLSX/OpenXLSXConfigVersion.cmake"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/OpenXLSX/OpenXLSXTargets.cmake")
    file(DIFFERENT EXPORT_FILE_CHANGED FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/OpenXLSX/OpenXLSXTargets.cmake"
         "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/build/CMakeFiles/Export/lib/cmake/OpenXLSX/OpenXLSXTargets.cmake")
    if(EXPORT_FILE_CHANGED)
      file(GLOB OLD_CONFIG_FILES "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/OpenXLSX/OpenXLSXTargets-*.cmake")
      if(OLD_CONFIG_FILES)
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/OpenXLSX/OpenXLSXTargets.cmake\" will be replaced.  Removing files [${OLD_CONFIG_FILES}].")
        file(REMOVE ${OLD_CONFIG_FILES})
      endif()
    endif()
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/OpenXLSX" TYPE FILE FILES "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/build/CMakeFiles/Export/lib/cmake/OpenXLSX/OpenXLSXTargets.cmake")
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^()$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/OpenXLSX" TYPE FILE FILES "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/build/CMakeFiles/Export/lib/cmake/OpenXLSX/OpenXLSXTargets-noconfig.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/mnt/c/Users/super/cpu-simulator-JiaaoLyu/OpenXLSX/build/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
