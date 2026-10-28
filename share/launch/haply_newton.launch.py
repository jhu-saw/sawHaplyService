"""Run the Haply console with the Newton patient cart."""

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
        candidate = Path(get_package_share_directory("haply_config")) / "dvrk_newton"
        if candidate.is_dir():
            directory = candidate
    except Exception:
        pass

    if directory is None:
        directory = Path(__file__).resolve().parent.parent / "dvrk_newton"

    profile = SystemProfile(
        config=directory / "newton.yaml",
        cart_scene=directory / "ECM_PSM1_PSM2_PSM3_mono.yaml",
        system_config=directory / "system-Haply-Newton.json",
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
        DeclareLaunchArgument("device", default_value="",
                              description="Warp device override"),
        OpaqueFunction(function=start_session, args=["dvrk_newton", profile]),
        TimerAction(period=2.0, actions=[
            ExecuteProcess(cmd=[
                "sh", "-c",
                "while ! grep -q '@dvrk:simulator:mono_source' /proc/net/unix; do "
                "sleep 0.5; "
                "done; "
                "exec gst-launch-1.0 unixfdsrc "
                "socket-path=dvrk:simulator:mono_source "
                "socket-type=abstract do-timestamp=true ! queue "
                "leaky=downstream max-size-buffers=1 ! videoconvert ! "
                "autovideosink sync=false",
            ], output="screen"),
        ], condition=IfCondition(LaunchConfiguration("camera_viewer"))),
    ])
