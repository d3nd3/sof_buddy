#pragma once

#include "feature_config.h"
#include <libloaderapi.h>
#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include <stdint.h>
#include "debug/callsite_classifier.h"

#ifdef Unknown
#undef Unknown
#endif

Module moduleForFnStart(uint32_t rva);

enum class StretchPicCaller {
	Unknown = 0,
	CtfFlagDraw,
	ControlFlagDraw,
	ScopeDraw,
	ExecuteLayoutString,
	CON_DrawConsole,
	VbarDraw,
	DrawStretchPic,
	loadbox_c_GetIndices,
	Draw_Line,
	ScopeCalcXY
};

StretchPicCaller getStretchPicCallerFrom(Module m, uint32_t fnStartRva);
StretchPicCaller getStretchPicCallerFromRva(uint32_t fnStartRva);

enum class PicCaller {
	Unknown = 0,
	ExecuteLayoutString,
	SCR_DrawCrosshair,
	NetworkDisconnectIcon,
	BackdropDraw,
	DrawPicWrapper
};

PicCaller getPicCallerFrom(Module m, uint32_t fnStartRva);
PicCaller getPicCallerFromRva(uint32_t fnStartRva);

enum class PicOptionsCaller {
	Unknown = 0,
	cInterface_DrawNum,
	cDMRanking_CalcXY,
	SCR_DrawCrosshair,
	spcl_DrawFPS,
};

PicOptionsCaller getPicOptionsCallerFrom(Module m, uint32_t fnStartRva);
PicOptionsCaller getPicOptionsCallerFromRva(uint32_t fnStartRva);

enum class CroppedPicCaller {
	Unknown = 0,
	cInventory2_Constructor,
};

CroppedPicCaller getCroppedPicCallerFrom(Module m, uint32_t fnStartRva);
CroppedPicCaller getCroppedPicCallerFromRva(uint32_t fnStartRva);

enum class FindImageCaller {
	Unknown = 0,
	Draw_Char, 
	Draw_StretchPic, 
	Draw_Line,
	LoadTexture, 
	GL_FindImage,
	InitGammaTable,
	R_ScrollDetail,
	Mod_LoadFaces,
	R_DrawAlphaQuad,
	R_DrawParticleList, 
	R_GetTerrainPolys,
	trITexture_c_load,
	R_MakeSkyVec
};

FindImageCaller getFindImageCallerFrom(Module m, uint32_t fnStartRva);
FindImageCaller getFindImageCallerFromRva(uint32_t fnStartRva);

enum class VertexCaller {
	Unknown = 0,
	Draw_Char,
	Draw_StretchPic,
	Draw_Line
};

VertexCaller getVertexCallerFromRva(Module m, uint32_t fnStartRva);

enum class FontCaller {
	Unknown = 0,
	ScopeCalcXY,
	DMRankingCalcXY,
	Inventory2,
	SCRDrawPause,
	SCR_DrawCenterPrint,
	RectDrawTextItem,
	RectDrawTextLine,
	TickerDraw,
	InputHandle,
	FileboxHandle,
	LoadboxGetIndices,
	ServerboxDraw,
	TipRender,
	DrawLine
};

FontCaller getFontCallerFrom(Module m, uint32_t fnStartRva);
FontCaller getFontCallerFromRva(uint32_t fnStartRva);

#endif
