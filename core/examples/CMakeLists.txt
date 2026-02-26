#
# (C) Copyright 2017-2024 Johns Hopkins University (JHU), All Rights Reserved.
#
# --- begin cisst license - do not edit ---
#
# This software is provided "as is" under an open source license, with
# no warranty.  The complete license can be found in license.txt and
# http://www.cisst.org/cisst/license.txt.
#
# --- end cisst license ---

cmake_minimum_required (VERSION 3.10)
project (sawHaplyServiceExamples VERSION 2.1.0)

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
find_package (cisst 1.2.0 REQUIRED ${REQUIRED_CISST_LIBRARIES})

# websocketpp dependency
find_package(websocketpp REQUIRED)

if (cisst_FOUND_AS_REQUIRED)

  # load cisst configuration
  include (${CISST_USE_FILE})

  # catkin/ROS paths
  cisst_set_output_path ()

  # sawHaplyService has been compiled within cisst, we should find it automatically
  find_package (sawHaplyService 2.0.0)

  if (sawHaplyService_FOUND AND CISST_HAS_QT)

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
                           )

    # link against cisst libraries (and dependencies)
    cisst_target_link_libraries (sawHaplyServiceQtExample ${REQUIRED_CISST_LIBRARIES})

    install (TARGETS sawHaplyServiceQtExample COMPONENT sawHaplyService-Examples
             RUNTIME DESTINATION bin
             LIBRARY DESTINATION lib
             ARCHIVE DESTINATION lib)

  else (sawHaplyService_FOUND AND CISST_HAS_QT)
    message ("Information: code in ${CMAKE_CURRENT_SOURCE_DIR} will not be compiled, it requires sawHaplyService and cisstQt")
  endif (sawHaplyService_FOUND AND CISST_HAS_QT)

endif (cisst_FOUND_AS_REQUIRED)
