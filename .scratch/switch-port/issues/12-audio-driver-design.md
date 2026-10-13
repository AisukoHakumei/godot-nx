# AudioDriverSwitch design

Type: grilling
Status: open
Blocked by: 06, 08
Map: ../map.md

## Question

Given the audio API pick, what is `AudioDriverSwitch`'s threading and buffering model: own thread calling `audio_server_process`, number and size of ring buffers, sample format conversion, latency target, and behaviour on init failure (fall back to the dummy driver)?
