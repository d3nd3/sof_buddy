# wine_focus

Wine 10's first alt-tab back often arrives as `WM_ACTIVATE` with the window active and still marked minimized. SoF then restores the window (`GLimp_AppActivate`) but leaves `ActiveApp` clear, so `Scr_UpdateScreen` does not draw and the client area stays gray until a second alt-tab.

Before each screen update, if the process is running under Wine and the game window is the foreground window, this sets `ActiveApp`, clears the minimized flag, and runs the same activate calls the engine uses for mouse and sound.

No cvars. Enabled from `features/FEATURES.txt`.
