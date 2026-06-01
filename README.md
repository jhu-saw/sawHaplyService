# sawHaplyService
SAW wrapper for Haply Inverse3 haptic devices. This repository provides a core component as well as:
* Example application with Qt based GUI
* ROS node (also with Qt based GUI)

# Links
 * License: http://github.com/jhu-cisst/cisst/blob/master/license.txt
 * JHU-LCSR software: http://jhu-lcsr.github.io/software/

# Dependencies
 * cisst libraries: https://github.com/jhu-cisst/cisst
 * Haply Service: The Haply service must be running and accessible via WebSockets (default: `ws://localhost:10001`).
 * Qt for user interface
 * ROS and ROS CRTK (optional) - works with ROS 1 and ROS 2!
 * libwebsocketpp: `sudo apt install libwebsocketpp-dev`
 
# Compilation and configuration

See https://github.com/jhu-saw/vcs for download and build instructions.

# Interfaces and frames

Each configured Haply device, for example `MTMR`, provides CRTK state in two
frames:

* `local/measured_cp`, `local/measured_cv`, and `local/measured_cs`: Haply
  service data in the local device base frame, named `<device>_base`.
* `measured_cp`, `measured_cv`, and `measured_cs`: the same data after applying
  `base_frame`, which defaults to an identity transform into the `user` frame.

The device interface also provides `base_frame` and `set_base_frame` so the
fixed transform from the local Haply frame to the user frame can be changed at
runtime.  Existing JSON `devices[].base_frame` entries are still accepted as the
initial base frame.

# Examples

## Main example

The main example provided is `sawHaplyServiceQtExample`.  The command line options are:
```sh
sawHaplyServiceQtExample:
 -j <value>, --json-config <value> : json configuration file (optional)
 -m, --component-manager : JSON files to configure component manager (optional)
 -D, --dark-mode : replaces the default Qt palette with darker colors (optional)
```

To run the example with a configuration file, use:
```sh
sawHaplyServiceQtExample -j myconfig.json
```

A default configuration file is installed in the `share` directory (`share/sawHaplyService/sawHaplyService-config.json`).

## ROS

If you also want to use the ROS node for ROS 1, run:
```sh
rosrun haply haply
```

For ROS 2, run:
```sh
ros2 run haply haply
```

## dVRK

To drive a simulated PSM with the Haply Inverse3.  In first terminal:

```sh
ros2 run dvrk_robot dvrk_system \
    -j "$(ros2 pkg prefix haply_config)/share/haply_config/system-MTMR-Haply-PSM1_KIN_SIMULATED-Teleop.json"
```

In second terminal, for visualization:

```sh
ros2 launch dvrk_model arm.launch.py arm:=PSM1 generation:=Classic simulated:=false
```

In RViz, make sure you rotate the scene so the PSM is facing you. This is the orientation assumed in `system-MTMR-Haply-PSM1_KIN_SIMULATED-Teleop.json`.

## Other "middleware"

Besides ROS, the Haply component can also stream data to your application using the *sawOpenIGTLink* or *sawSocketStreamer* components.  See:
* [sawOpenIGTLink](https://github.com/jhu-saw/sawOpenIGTLink)
* [sawSocketStreamer](https://github.com/jhu-saw/sawSocketStreamer)
