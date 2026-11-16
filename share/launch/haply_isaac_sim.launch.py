"""Run the Haply console with the Isaac Sim patient cart."""

from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from dvrk_simulator_base.launch import SystemProfile, start_session
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, OpaqueFunction, TimerAction
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    directory = None
    try:
        candidate = Path(get_package_share_directory("haply_config")) / "dvrk_simulation"
        if candidate.is_dir():
            directory = candidate
    except Exception:
        pass

    if directory is None:
        directory = Path(__file__).resolve().parent.parent / "dvrk_simulation"

    profile = SystemProfile(
        config=directory / "isaac_sim.yaml",
        cart_scene=directory / "ECM_PSM1_PSM2_PSM3_mono_rtsp.yaml",
        system_config=directory / "system-Haply.json",
        cwd=directory,
    )

    return LaunchDescription([
        DeclareLaunchArgument("config", default_value=str(profile.config),
                              description="Backend runtime YAML"),
        DeclareLaunchArgument("exercise", default_value="tray_cubes.yaml",
                              description="Scene/exercise YAML path or installed filename"),
        DeclareLaunchArgument("scene", default_value=LaunchConfiguration("exercise")),
        DeclareLaunchArgument("headless", default_value="false",
                              description="Desktop viewer override; empty uses runtime YAML"),
        DeclareLaunchArgument("camera_viewer", default_value="true",
                              description="Start a mono GStreamer camera viewer"),
        DeclareLaunchArgument("rqt", default_value="false",
                              description="Start the dVRK monitor"),
        DeclareLaunchArgument("rqt_console", default_value="false",
                              description="Include console monitoring"),
        DeclareLaunchArgument("console", default_value="console",
                              description="Console namespace for clients"),
        OpaqueFunction(function=start_session, args=["dvrk_isaac_sim", profile]),
        TimerAction(period=2.0, actions=[
            ExecuteProcess(cmd=[
                "sh", "-c",
                # Isaac startup/asset conversion can take longer than the timer.
                # Reconnect until its native RTSP server is ready, or after a drop.
                "while true; do "
                "gst-launch-1.0 rtspsrc location=rtsp://localhost:8554/ECM "
                "protocols=tcp latency=0 ! decodebin ! videoconvert ! "
                "autovideosink sync=false; "
                "sleep 2; "
                "done",
            ], output="screen"),
        ], condition=IfCondition(LaunchConfiguration("camera_viewer"))),
    ])
