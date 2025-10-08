#pragma once

// Module-specific hook initialization functions
extern "C" void InitializeExeHooks(void);     // SoF.exe hooks (0x200xxxxx)
extern "C" void InitializeRefHooks(void);     // ref.dll hooks (0x5xxxxxxx)  
extern "C" void InitializeGameHooks(void);    // game.dll hooks (0x5xxxxxxx)
extern "C" void InitializePlayerHooks(void);  // player.dll hooks (0x5xxxxxxx)

// Legacy function (use module-specific versions instead)
extern "C" void InitializeAllHooks(void);
extern "C" void ShutdownAllHooks(void);