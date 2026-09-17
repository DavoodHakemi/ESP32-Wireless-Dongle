"""PlatformIO compatibility helpers for Arduino-ESP32 3.0.7.

The pioarduino 51.03.07 framework package contains the Arduino-ESP32 3.0.7
Networking library, but PlatformIO's LDF may fail to expose it to the build
when WiFi.h includes Network.h. We use the exact framework-installed copy
instead of duplicating the library in the project.
"""

from pathlib import Path

Import("env")

FRAMEWORK_PACKAGE = env.PioPlatform().get_package_dir(
    "framework-arduinoespressif32"
)

if not FRAMEWORK_PACKAGE:
    raise RuntimeError(
        "framework-arduinoespressif32 package directory could not be resolved"
    )

NETWORK_SRC = Path(FRAMEWORK_PACKAGE) / "libraries" / "Network" / "src"
if not NETWORK_SRC.is_dir():
    raise RuntimeError(
        "Arduino-ESP32 3.0.7 Network library was not found at: %s"
        % NETWORK_SRC
    )

# Make Network.h and the other Networking headers visible to WiFi and project code.
env.Append(CPPPATH=[str(NETWORK_SRC)])

# Build the exact framework Networking sources because the LDF can otherwise
# omit this framework library even though WiFi depends on it.
NETWORK_BUILD_DIR = Path(env.subst("$BUILD_DIR")) / "framework-network"
env.BuildSources(str(NETWORK_BUILD_DIR), str(NETWORK_SRC))

print("[ESP32 Wireless Dongle] Arduino-ESP32 Network library enabled from:")
print("[ESP32 Wireless Dongle]   %s" % NETWORK_SRC)
