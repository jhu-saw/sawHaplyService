#
# (C) Copyright 2016-2026 Johns Hopkins University (JHU), All Rights Reserved.
#
# --- begin cisst license - do not edit ---
#
# This software is provided "as is" under an open source license, with
# no warranty.  The complete license can be found in license.txt and
# http://www.cisst.org/cisst/license.txt.
#
# --- end cisst license ---

cmake_minimum_required (VERSION 3.10)
project (sawHaplyService VERSION 1.0.0)

# create a list of required cisst libraries
set (REQUIRED_CISST_LIBRARIES cisstCommon
                              cisstVector
                              cisstOSAbstraction
                              cisstMultiTask
                              cisstParameterTypes)

# websocketpp dependency
find_package(websocketpp REQUIRED)

find_package (cisst 1.5 REQUIRED COMPONENTS ${REQUIRED_CISST_LIBRARIES})

if (NOT CISST_HAS_JSON)
  message( FATAL_ERROR "sawHaplyService requires cisst to be built with JSON" )
endif ()

# catkin/ROS paths
cisst_set_output_path ()


# create/configure file for find_package (sawHaplyService)
set (sawHaplyService_INCLUDE_DIR
     "${sawHaplyService_SOURCE_DIR}/include"
     "${sawHaplyService_BINARY_DIR}/include")
set (sawHaplyService_HEADER_DIR "${sawHaplyService_SOURCE_DIR}/include/sawHaplyService")
set (sawHaplyService_LIBRARY_DIR "${LIBRARY_OUTPUT_PATH}")
set (sawHaplyService_LIBRARIES sawHaplyService websocketpp::websocketpp)

include_directories (${sawHaplyService_INCLUDE_DIR})

# add Qt code
if (CISST_HAS_QT)
  add_subdirectory (code/Qt)
  set (sawHaplyService_LIBRARIES ${sawHaplyService_LIBRARIES} sawHaplyServiceQt)
endif (CISST_HAS_QT)

# add all config files for this component
cisst_add_config_files (sawHaplyService)

add_library (sawHaplyService
             ${sawHaplyService_HEADER_DIR}/sawHaplyServiceExport.h
             code/mtsHaply.cpp
             ${sawHaplyService_HEADER_DIR}/mtsHaply.h)
set_target_properties (sawHaplyService PROPERTIES
                       VERSION ${sawHaplyService_VERSION}
                       FOLDER "sawHaplyService")
target_link_libraries (sawHaplyService PUBLIC
                       ${cisst_LIBRARIES})

# Install target for headers and library
install (DIRECTORY
         "${sawHaplyService_SOURCE_DIR}/include/sawHaplyService"
         DESTINATION include
         COMPONENT sawHaplyService-dev)

install (TARGETS sawHaplyService COMPONENT sawHaplyService
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib)
