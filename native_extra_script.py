"""
native_extra_script.py  (PRE-type extra_script)
Builds src/*.c files into a static library and links it into every native
test binary.  Excludes the Arduino-only .cpp files that use WiFiClientSecure
and Arduino.h — those are handled by atsdk_atsdk.cpp / atsdk_socket.cpp on
real hardware only.
"""

Import("env")

env.BuildLibrary(
    ".pio/build/${PIOENV}/atsdk_lib",  # where to put the .a
    "$PROJECT_DIR/src",               # source directory
    [
        "+<*.c>",                      # include all root-level .c files
        "-<atsdk_atsdk.cpp>",          # exclude Arduino Serial
        "-<atsdk_socket.cpp>",         # exclude WiFiClientSecure
    ],
)
