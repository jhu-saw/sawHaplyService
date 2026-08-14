#
# (C) Copyright 2017-2026 Johns Hopkins University (JHU), All Rights Reserved.
#
# --- begin cisst license - do not edit ---
#
# This software is provided "as is" under an open source license, with
# no warranty.  The complete license can be found in license.txt and
# http://www.cisst.org/cisst/license.txt.
#
# --- end cisst license ---

cmake_minimum_required (VERSION 3.10)
project (sawHaplyServiceExamples VERSION 1.0.0)

# create a list of required cisst libraries
set (REQUIRED_CISST_LIBRARIES cisstCommon
                              cisstCommonQt
                              cisstVector
                              cisstVectorQt
                              cisstOSAbstraction
                              cisstMultiTask
                              cisstMultiTaskQt
                              cisstParameterTypes)

# find cisst and make sure the required libraries have been compiled
find_package (cisst 1.2.0 REQUIRED COMPONENTS ${REQUIRED_CISST_LIBRARIES})

# websocketpp dependency
find_package(websocketpp REQUIRED)

# catkin/ROS paths
cisst_set_output_path ()

# sawHaplyService has been compiled within cisst, we should find it automatically
find_package (sawHaplyService 0.1.0 REQUIRED)

if (CISST_HAS_QT)

  # sawHaplyService configuration
  include_directories (${sawHaplyService_INCLUDE_DIR})
  link_directories (${sawHaplyService_LIBRARY_DIR})

  add_executable (sawHaplyServiceQtExample main.cpp)
  set_target_properties (sawHaplyServiceQtExample PROPERTIES
                         COMPONENTS sawHaplyService-Examples
                         FOLDER "sawHaplyService")

  # link against non cisst libraries and saw components
  target_link_libraries (sawHaplyServiceQtExample
                         ${sawHaplyService_LIBRARIES}
                         ${sawHaplyServiceQt_LIBRARIES}
                         ${QT_LIBRARIES}
                         ${cisst_LIBRARIES})

  install (TARGETS sawHaplyServiceQtExample COMPONENT sawHaplyService-Examples
           RUNTIME DESTINATION bin
           LIBRARY DESTINATION lib
           ARCHIVE DESTINATION lib)

endif (CISST_HAS_QT)
