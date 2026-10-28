# Haply + Newton Example

## Mounting

Mount both Haply Inverse3 devices with their LEDs facing the user.
The base-frame transforms in
`sawHaplyService-JHU-Imerse-0699-069E.json` assume this mounting arrangement.

## Verse Grip Orientation Reset

Directions below are from the user's perspective. Before resetting each
Verse Grip's orientation, hold it in the corresponding pose:

| Grip | Stylus Direction | Button Orientation |
| --- | --- | --- |
| Right (`MTMR`) | Point to the user's right | Buttons on top |
| Left (`MTML`) | Point to the user's left | Buttons on top |

Reset each grip's orientation while holding that pose.

## Launch

With Haply Service running, start the example with:

```sh
ros2 launch haply_config haply_newton.launch.py
```
