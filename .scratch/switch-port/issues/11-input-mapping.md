# Input mapping: libnx HID to Godot Input

Type: grilling
Status: open
Blocked by: 08
Map: ../map.md

## Question

How do libnx `PadState` buttons/sticks map to Godot `JoyButton`/`JoyAxis` (and the SDL-style GUID so the gamepad DB works), how are handheld, dual Joy-Con, single horizontal Joy-Con, and Pro Controller configurations represented as Godot devices (one device per player?), how does touch become `InputEventScreenTouch`/`Drag`, and is six-axis mapped to `Input::set_accelerometer/gyroscope` in the first milestone?
