# Haply with dVRK Simulators

These examples use two Haply Inverse3 devices and Verse Grips to control an
ECM/PSM1/PSM2/PSM3 patient cart in Newton, Isaac Sim, or PyBullet. All three
backends share the Haply device configuration, PCsensor foot pedals, and dVRK
system configuration in this directory.

## Setup

Retrieve and build `sawHaplyService`, `sawGenericInput`, the selected simulator,
and their dependencies in the same ROS 2 workspace, then source that workspace.
The examples require `sawHaplyServiceCore`, `sawGenericInputCore`, `haply_config`,
and the selected backend package (`dvrk_newton`, `dvrk_isaac_sim`, or
`dvrk_pybullet`). `sawGenericInputCore` is optional for the Haply component
itself and required by these pedal-based examples.

Before launching, start Haply Service, check the device serials in
`sawHaplyService-JHU-Imerse-0699-069E.json`, and set up pedal permissions below.
Run only one simulator profile or pedal monitor at a time.

## Launch

Each launch starts the Haply dVRK system, the patient cart, the `tray_cubes.yaml`
exercise, and a mono GStreamer camera viewer. Home and enable teleoperation in
the dVRK system GUI, then hold the operator-present pedal. Haply A closes the
gripper and B opens it; the foot pedals provide clutch and camera inputs.

### Newton

```sh
ros2 launch haply_config haply_newton.launch.py
```

Runtime settings are in `newton.yaml`. The `device` launch argument overrides
the Warp device, for example `device:=cpu`.

### Isaac Sim

Select the Isaac installation with `ISAAC_SIM_DIR` (containing `python.sh`), or
set `DVRK_ISAAC_SIM_PYTHON` to the worker interpreter.

```sh
ros2 launch haply_config haply_isaac_sim.launch.py
```

Runtime settings are in `isaac_sim.yaml`. The camera streams raw RTSP at
`rtsp://localhost:8554/ECM`; the viewer retries during startup and after
connection loss. GStreamer needs `rtspsrc`, `rtpvrawdepay`, `decodebin`,
`videoconvert`, and `autovideosink` plugins. If you change the RTSP port or mount
path in the cart scene, update the viewer URL in the launch file.

Isaac Sim currently renders kinematic PSMs without Newton's grasp attachment
behavior.

### PyBullet

Set up the backend Python environment using
`dvrk_pybullet/scripts/bootstrap_venv.sh` if needed. Select the worker with
`DVRK_PYBULLET_PYTHON`; a workspace `.venv-pybullet` is also discovered
automatically.

```sh
ros2 launch haply_config haply_pybullet.launch.py
```

Runtime settings are in `pybullet.yaml`. The default camera renderer is EGL;
set `renderer: tiny` in that YAML if EGL is unavailable. Newton and PyBullet
use the GStreamer `unixfdsrc` plugin for their camera viewer.

### Launch options

All three launches accept `config`, `exercise` (or `scene`), `headless`,
`camera_viewer`, `rqt`, `rqt_console`, and `console`. Newton additionally accepts
`device`. For example:

```sh
ros2 launch haply_config haply_newton.launch.py headless:=true camera_viewer:=false rqt:=true
```

Set `camera_viewer:=false` to use another camera client. `headless:=true`
disables the simulator's desktop viewer; the camera viewer is controlled
separately. An empty `headless` argument uses the runtime YAML setting.

## Shared configuration

| File | Purpose |
| --- | --- |
| `system-Haply.json` | Haply and ROS arm components, pedal connections, and PSM/ECM teleoperation |
| `sawHaplyService-JHU-Imerse-0699-069E.json` | Device serials and mounting transforms for both Haply arms |
| `sawGenericInput-pedals.json` | PCsensor foot pedal selection and button mappings |
| `ECM_PSM1_PSM2_PSM3_mono.yaml` | Newton/PyBullet cart poses and mono camera with Unix FD transport |
| `ECM_PSM1_PSM2_PSM3_mono_rtsp.yaml` | Isaac Sim cart poses and mono camera with RTSP transport |

The launch files use this directory as the system's working directory, so the
component configuration filenames in `system-Haply.json` resolve here. Edit
the shared device files once to change all three examples. The cart scenes
have identical arm mounting poses; their camera transports differ.

## Mounting

Mount both Haply Inverse3 devices with their LEDs facing the user. Set the
mounting transforms in `sawHaplyService-JHU-Imerse-0699-069E.json`, under
`devices[].base_frame.transform`. dVRK ignores the system-level arm base frame
for generic arms such as Haply.

The supplied transforms place MTML at X = +0.80 m and MTMR at X = -0.80 m
in the common `user` reference frame, giving the ECM handlebar a 1.60 m base
separation. Both have a common Z = +0.60 m translation, where +Z is forward.
This supplies an initial forward distance for the two-hand ECM mapping, which
uses the direction of the hand midpoint from the user-frame origin. The common
translation preserves the hand separation.

The measured midpoint also includes the transformed local Haply positions.
To target a 0.60 m midpoint, adjust the common Z translation by
`0.60 - measured_midpoint_Z` from its current value. Re-enable camera
teleoperation after changing calibration so its initial hand geometry is
captured again.

## Foot pedals

The system loads `sawGenericInput` using `sawGenericInput-pedals.json`. It
selects the PCsensor FootSwitch Keyboard (USB 3553:b001), captures its keys
exclusively, and connects standard cisst `Button` events to the console:

| Pedal key | Provided interface | System required interface |
| --- | --- | --- |
| `KEY_A` | `Pedals/Clutch` | `console/required_clutch` |
| `KEY_B` | `Pedals/Camera` | `console/required_camera` |
| `KEY_C` | `Pedals/OperatorPresent` | `console/required_operator_present` |

A physical press emits `PRESSED`; release emits `RELEASED`. Repeated
characters are ignored. A clutch/camera press lasting at most 300 ms additionally
emits `CLICKED` after release; clutch quick taps cycle selected PSM teleoperation.
Operator presence has quick-tap detection disabled. Edit `quick_tap_ms` to
adjust or disable click detection.

Install the device-specific permissions rule from the `sawGenericInput` source
directory:

```sh
sudo install -m 0644 core/share/udev/70-saw-evdev-pcsensor.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```

Unplug/replug the pedals afterward. The rule grants the active local seat user
access without running the dVRK system as root. Before launching dVRK, check
hold timing and keyboard suppression with:

```sh
sawGenericInputQtExample -j /path/to/sawHaplyService/share/dvrk_simulation/sawGenericInput-pedals.json
```

Only run one component/example against the pedals at a time. Exclusive capture
suppresses pedal letters in desktop applications without requiring terminal
focus. On disconnect the component releases all inputs, including operator
presence, and retries once per second. After reconnecting, release any held
pedal before pressing it again. The component reports missing devices,
permissions, or grab errors through its `Device` interface and cisst log.
See the [sawGenericInput README](https://github.com/jhu-saw/sawGenericInput)
for stable device paths and full configuration details.

## Verse Grip orientation reset

Directions below are from the user's perspective. Before resetting each
Verse Grip's orientation, hold it in the corresponding pose:

| Grip | Stylus direction | Button orientation |
| --- | --- | --- |
| Right (`MTMR`) | Point to the user's right | Buttons on top |
| Left (`MTML`) | Point to the user's left | Buttons on top |

Reset each grip's orientation while holding that pose. Hold the VerseGrip C
button for three seconds. The component does not report orientation-reset
completion; this requires an event from the Haply API.
