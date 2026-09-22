# wine_focus

Wine 10's first alt-tab back delivers `WM_ACTIVATE` with the window active and still marked minimized. SoF shows the window (`GLimp_AppActivate` → `ShowWindow(SW_RESTORE)`) but leaves `ActiveApp` clear. That restore nests another `WM_ACTIVATE` which takes the inactive path, so `Scr_UpdateScreen` sleeps and the client area stays the gray window-class brush until a later clean activate.

This subclasses the game window and:

- clears the minimized bit on an activating `WM_ACTIVATE` so `MainWndProc` takes the active path
- drops `WM_ACTIVATE` nested inside that call, so `ShowWindow` cannot clear `ActiveApp` again
- rebinds the current OpenGL context (`wglMakeCurrent`) so Wine recreates the drawable lost while minimized

If the engine is still inactive while the window is foreground or on screen (not parked at -32000), the next `Scr_UpdateScreen` marks the app active, runs the activate functions, and rebinds GL.

No cvars. Enabled from `features/FEATURES.txt`.
