# Cinematic Freeze Fix

- Hook: CinematicFreeze(bool)
- Reason: Maintain cl.frame.cinematicFreeze state immediately to prevent black loading screen after cinematics.
- Files:
  - hooks.cpp: feature-specific behavior called from the hook in core (my_CinematicFreeze).
