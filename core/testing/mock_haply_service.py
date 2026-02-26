import asyncio
import json
import websockets

# Simulated state of the Inverse3
state = {
    "inverse3": [
        {
            "device_id": "0",
            "state": {
                "cursor_position": {"x": 0.0, "y": 0.05, "z": 0.0},
                "cursor_velocity": {"x": 0.0, "y": 0.0, "z": 0.0},
                "angular_position": {"x": 0.0, "y": 0.0, "z": 0.0},
                "angular_velocity": {"x": 0.0, "y": 0.0, "z": 0.0}
            }
        }
    ]
}

async def handle_client(websocket):
    print(f"Client connected from {websocket.remote_address}")
    try:
        async for message in websocket:
            # 1. Receive commands from the SDK
            data = json.loads(message)
            
            # 2. Log received commands for debugging
            if "inverse3" in data:
                for device in data["inverse3"]:
                    if "commands" in device:
                        cmds = device["commands"]
                        if "set_cursor_force" in cmds:
                            force = cmds["set_cursor_force"]["values"]
                            print(f"Force Command -> X: {force['x']:.3f}, Y: {force['y']:.3f}, Z: {force['z']:.3f}")
                        elif "probe_position" in cmds:
                            # SDK is just asking for data
                            pass

            # 3. Send the current simulated state
            await websocket.send(json.dumps(state))

            # Maintain a small delay to simulate real service frequency (e.g., 100Hz for testing)
            await asyncio.sleep(0.01)
            
    except websockets.exceptions.ConnectionClosed:
        print("Client disconnected.")

async def main():
    print("Starting Mock Haply Service on ws://localhost:10001")
    async with websockets.serve(handle_client, "localhost", 10001):
        await asyncio.Future()  # Run forever

if __name__ == "__main__":
    asyncio.run(main())
