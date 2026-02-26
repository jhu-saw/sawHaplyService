# Testing with Mock Haply Service

This directory contains a mock service to test the **sawHaplyService** and its ROS wrapper without physical hardware.

## Prerequisites

The mock service requires the `websockets` Python library. You can install it using `apt`:

```bash
sudo apt update
sudo apt install python3-websockets
```

---

## 1. Start the Mock Service

Run the mock service in its own terminal. It will listen on `ws://localhost:10001` (the default for Haply services).

```bash
python3 /home/anton/wss/haply/src/cisst-saw/sawHaplyService/core/testing/mock_haply_service.py
```

It should output:
`Starting Mock Haply Service on ws://localhost:10001`

---

## 2. Test the C++ Standalone Example

In a new terminal, run the compiled Qt example:

```bash
/home/anton/wss/haply/build/sawHaplyServiceCore/examples/sawHaplyServiceQtExample
```

You should see:
- A Qt window showing device state.
- **Mock Service Terminal:** Logs "Client connected" and optionally force commands if you interact with the UI.

---

## 3. Test the ROS 2 Bridge

Ensure you have sourced your ROS 2 environment (e.g., Jazzy) and the local workspace:

```bash
source /opt/ros/jazzy/setup.bash
source /home/anton/wss/haply/install/setup.bash
```

Run the `haply` ROS node:
```bash
ros2 run haply haply
```

To verify data is flowing in ROS, you can echo the measured position topic in another terminal:
```bash
ros2 topic echo /HaplySDK/Test/measured_cp
```

*(Note: The topic name depends on your configuration file; "Test" is the default hardcoded name for now.)*
