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

# Compilation and configuration

See https://github.com/jhu-saw/vcs for download and build instructions.

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

## Other "middleware"

Besides ROS, the Haply component can also stream data to your application using the *sawOpenIGTLink* or *sawSocketStreamer* components.  See:
* [sawOpenIGTLink](https://github.com/jhu-saw/sawOpenIGTLink)
* [sawSocketStreamer](https://github.com/jhu-saw/sawSocketStreamer)
