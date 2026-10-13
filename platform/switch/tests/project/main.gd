extends Node3D

@onready var box: MeshInstance3D = $Box
@onready var scissor: MeshInstance3D = $Scissor
@onready var label: Label = $UI/Label
@onready var line_edit: LineEdit = $UI/LineEdit
@onready var tone: AudioStreamPlayer = $Tone

var tone_playback: AudioStreamGeneratorPlayback
var tone_phase := 0.0

var elapsed := 0.0


func _ready() -> void:
	# A disc with a hard alpha edge; ALPHA_SCISSOR turns that into `discard`.
	var img := Image.create(64, 64, false, Image.FORMAT_RGBA8)
	for y in 64:
		for x in 64:
			var inside := Vector2(x - 32, y - 32).length() < 24.0
			img.set_pixel(x, y, Color(1.0, 0.5, 0.1, 1.0 if inside else 0.0))
	var mat := scissor.get_surface_override_material(0) as StandardMaterial3D
	mat.albedo_texture = ImageTexture.create_from_image(img)
	print("Video adapter: ", RenderingServer.get_video_adapter_name(), " / ", RenderingServer.get_video_adapter_api_version())
	print("Window: ", DisplayServer.window_get_size(), " locale: ", OS.get_locale())
	tone.play()
	tone_playback = tone.get_stream_playback()
	print("Audio driver: ", AudioServer.get_driver_name(), " mix rate ", AudioServer.get_mix_rate())


func _process(delta: float) -> void:
	elapsed += delta
	box.rotation.y = elapsed
	box.rotation.x = elapsed * 0.3
	_fill_tone()
	label.text = "FPS %d  window %s  frame %.1f ms" % [Engine.get_frames_per_second(), DisplayServer.window_get_size(), delta * 1000.0]


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventJoypadButton:
		print("joy button ", event.button_index, " pressed=", event.pressed)
		if event.pressed and event.button_index == JOY_BUTTON_B:
			# Switch A is Godot B. Rumble: weak 0.5, strong 1.0 for 0.3 s.
			Input.start_joy_vibration(0, 0.5, 1.0, 0.3)
		if event.pressed and event.button_index == JOY_BUTTON_A:
			# Switch B is Godot A: toggle the tone.
			if tone.playing:
				tone.stop()
			else:
				tone.play()
				tone_playback = tone.get_stream_playback()
		if event.pressed and event.button_index == JOY_BUTTON_Y:
			# Switch X is Godot Y: focus the text box, which opens the keyboard.
			line_edit.grab_focus()
	elif event is InputEventJoypadMotion:
		if absf(event.axis_value) > 0.2 or event.axis_value == 0.0:
			print("joy axis ", event.axis, " = %.2f" % event.axis_value)
	elif event is InputEventScreenTouch:
		print("touch ", event.index, " pressed=", event.pressed, " at ", event.position)
	elif event is InputEventScreenDrag:
		print("drag ", event.index, " at ", event.position)


func _fill_tone() -> void:
	if tone_playback == null or not tone.playing:
		return
	var frames := tone_playback.get_frames_available()
	for i in frames:
		tone_phase = fmod(tone_phase + 440.0 / 48000.0, 1.0)
		var v := sin(TAU * tone_phase)
		tone_playback.push_frame(Vector2(v, v))
