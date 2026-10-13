# Create the `switch-4.7` branch from `4.7.2-stable`

Type: task
Status: claimed
Blocked by: (none)
Map: ../map.md

## Question

Fetch upstream `godotengine/godot` tag `4.7.2-stable` into this fork and create branch `switch-4.7` from it, so every later ticket reads the 4.7 platform code rather than master (4.8 dev). The `.scratch/` tracker and `CONTEXT.md` are untracked and carry over; commit them on the new branch. Record the resulting commit hash and confirm `scons platform=list` style discovery still works on that tag. AFK: the agent can do this alone.
