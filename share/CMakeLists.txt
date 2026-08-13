#
# (C) Copyright 2026 Johns Hopkins University (JHU), All Rights Reserved.
#
# --- begin cisst license - do not edit ---
#
# This software is provided "as is" under an open source license, with
# no warranty.  The complete license can be found in license.txt and
# http://www.cisst.org/cisst/license.txt.
#
# --- end cisst license ---

cmake_minimum_required (VERSION 3.16)
project (haply_config VERSION 2.1.0)

# ROS 1
find_package (catkin QUIET)
if (catkin_FOUND)
  catkin_package ()
else (catkin_FOUND)

  # ROS 2
  find_package (ament_cmake QUIET)
  if (ament_cmake_FOUND)
    install (FILES
      sawHaplyService-JHU-Imerse-0699.json
      sawHaplyService-JHU-Imerse.json
      system-MTMR-Haply-PSM1_KIN_SIMULATED-Teleop.json
      DESTINATION share/${PROJECT_NAME})

    ament_package ()
  endif (ament_cmake_FOUND)

endif (catkin_FOUND)
