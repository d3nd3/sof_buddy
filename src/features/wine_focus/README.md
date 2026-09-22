# wine_focus

Wine's first alt-tab back delivers `WM_ACTIVATE` with the window active and still marked minimized. SoF shows the window (`GLimp_AppActivate`) but leaves `ActiveApp` clear, so `Scr_UpdateScreen` does not draw and the client area stays gray until a later clean activate.

This subclasses the game window and clears that minimized bit before `MainWndProc` runs, so the engine takes the same path as the second alt-tab. If the window is already in that stuck state (engine minimized flag set, window actually visible), the next screen update marks the app active and calls the same activate functions, including `GLimp_AppActivate`.

No cvars. Enabled from `features/FEATURES.txt`.
