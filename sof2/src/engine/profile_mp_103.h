#pragma once

#include <stdint.h>

struct Sof2Mp103Profile {
	static constexpr uint32_t kImageSize = 0x00E8F000u;
	static constexpr uint32_t kRvaComPrintf = 0x00046240u;   /* Com_Printf @ VA 0x446240 */
	static constexpr uint32_t kRvaCvarGet = 0x0004B130u;    /* Cvar_Get @ VA 0x44B130 */
	static constexpr uint32_t kRvaCvarSet2 = 0x0004B6D0u;   /* Cvar_Set2 @ VA 0x44B6D0 */
	static constexpr uint32_t kRvaCbufAddText = 0x00045250u;
	static constexpr uint32_t kRvaCbufExecute = 0x00045460u;
	static constexpr uint32_t kCbufExecuteHookLen = 6u;
	static constexpr uint32_t kRvaCmdAddCommand = 0x00045AE0u;
	static constexpr uint32_t kRvaCmdExecuteString = 0x00045C30u;
	static constexpr uint32_t kRvaCmdArgc = 0x00045780u;
	static constexpr uint32_t kRvaCmdArgv = 0x00045790u;
	static constexpr uint32_t kRvaCmdRemoveCommand = 0x00045B80u;
	static constexpr uint32_t kRvaCmdArgvBuffer = 0x00045800u;
	static constexpr uint32_t kRvaComDPrintf = 0x00046430u;
	static constexpr uint32_t kRvaComSprintf = 0x00045ABA0u;
	static constexpr uint32_t kRvaComError = 0x000464C0u;
	static constexpr uint32_t kRvaSysMilliseconds = 0x00046A8D0u;
	static constexpr uint32_t kRvaCvarSet = 0x0004BA70u;
	static constexpr uint32_t kRvaCvarSetFloat = 0x0004BAB0u; /* Cvar_Set from float @ VA 0x44BAB0 */
	static constexpr uint32_t kRvaCmdArgs = 0x00045940u;      /* Cmd_ArgsBuffer-style concat */
	static constexpr uint32_t kRvaKeyCatchers = 0x0004F202Cu; /* dword_8F202C menu/key flags */
	static constexpr uint32_t kRvaClsState = 0x0004F2028u; /* dword_8F2028 cls.state (int) */
	static constexpr uint32_t kRvaKeyState = 0x00048DC48u;    /* dword_88DC48 key down table */
	static constexpr uint32_t kRvaZMalloc = 0x000447030u; /* Z_Malloc @ VA 0x447030 */
	static constexpr uint32_t kRvaZFree = 0x000447170u;   /* Z_Free @ VA 0x447170 */
	static constexpr uint32_t kRvaGlConfigVidWidth = 0x00057AFF8u;
	static constexpr uint32_t kRvaGlConfigVidHeight = 0x00057AFFCu;
	static constexpr uint32_t kRvaFSReadFile = 0x0004EFA0u;
	static constexpr uint32_t kRvaFSFreeFile = 0x0004F280u;
	static constexpr uint32_t kRvaRefGlExports = 0x0016A9B8u; /* ref export table copied by R_Init */
	static constexpr uint32_t kRvaCvarHashHead = 0x00073E960u; /* dword_B3E960 */
	/* 1fx MP gold — GfxInfo_f: skip GL_EXTENSIONS ri.Printf @ VA 0x4BB342, add esp 0x38 @ 0x4BB359 */
	static constexpr uint32_t kGfxSkipRva = 0x000BB342u;
	static constexpr uint32_t kGfxEspAddRva = 0x000BB359u;
	/* r_mode Cvar_Get float bounds @ VA 0x4BA615 (max 11.0) / 0x4BA61A (min -1.0) */
	static constexpr uint32_t kRModeMaxRva = 0x000BA615u;
	static constexpr uint32_t kRModeMinRva = 0x000BA61Au;
	/* R_Register: push imm for r_mode default "3" @ 0x4BA624 → "-1" @ VA 0x564AFC */
	static constexpr uint32_t kRModeDefaultImmRva = 0x000BA624u;
	static constexpr uint32_t kRModeDefaultMinusOneVa = 0x00564AFCu;
	/* master.sof2.ravensoft.com → master.1fxmod.org @ VA 0x55A9AC */
	static constexpr uint32_t kMasterRva = 0x0015A9ACu;
	static constexpr uint32_t kVidModeTableRva = 0x0016C358u;     /* r_vidModeTable (Q3 r_vidModes[]) */
	static constexpr uint32_t kVidModeCountRva = 0x0016C418u;     /* r_vidModeCount */
	static constexpr uint32_t kVidModeResolveAddRva = 0x000B98EFu; /* R_GetModeInfo: add eax, table */
	static constexpr uint32_t kVidModelistEdiRva = 0x000BB28Du;   /* R_ModeList_f: mov edi, table */
	static constexpr uint32_t kRGetModeInfoRva = 0x000B98A0u;     /* R_GetModeInfo @ VA 0x4B98A0 */
	static constexpr uint32_t kRGetModeInfoHookLen = 7u;          /* mov eax,[esp+0Ch]; cmp eax,-1 */
	static constexpr uint32_t kRvaClFrame = 0x00040FF60u;         /* CL_Frame @ VA 0x40FF60 */
	static constexpr uint32_t kClFrameHookLen = 6u;               /* push ecx; mov eax, cl ptr */
};
