# Display and docked/handheld resolution lifecycle

Type: grilling
Status: open
Blocked by: 05, 08
Map: ../map.md

## Question

How does `DisplayServerSwitch` report window size and handle the docked/handheld switch: poll `appletGetOperationMode`, listen via `appletHook`, resize the EGL surface or let `nwindowSetDimensions` + Godot stretch mode cover it? Which `DisplayServer` features are advertised (no multiple windows, no cursor, touch yes)? What is the frame-pacing/vsync model (`eglSwapInterval`, `appletSetFocusHandlingMode`)?

Context: Godot's generic `EGLManager` cannot be used (hard-linked platform-display symbols absent in mesa 20.1); a Switch GL manager is needed, and `eglQuerySurface` returns 0 so size must come from `nwindow`/applet APIs; see [research/05-gpu-stack.md](../research/05-gpu-stack.md).
Context: the platform-layer ticket fixed the display server's scope (single window, touch, swkbd, no cursor/multi-window/IME/clipboard) and that it owns `JoypadSwitch` polling; this ticket decides the resize and vsync mechanics only.
