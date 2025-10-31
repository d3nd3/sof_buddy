#pragma once

#include "feature_config.h"
#include <libloaderapi.h>
#if FEATURE_UI_SCALING

#include <stdint.h>
#include <initializer_list>
#include "callsite_classifier.h"

// Guard against Windows headers defining macros like 'Unknown'
#ifdef Unknown
#undef Unknown
#endif

// Centralized caller enums and RVA mapping helpers

	inline Module moduleForFnStart(uint32_t rva) {
		for (Module m : {Module::SofExe, Module::RefDll, Module::PlayerDll, Module::GameDll, Module::SpclDll})
			if (CallsiteClassifier::hasFunctionStart(m, rva)) return m;
		return Module::Unknown;
	}

/*
    StretchPic
*/
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

inline StretchPicCaller getStretchPicCallerFrom(Module m, uint32_t fnStartRva) {
	switch (m) {
		case Module::SofExe:
			switch (fnStartRva) {

				case 0x00020F90: return StretchPicCaller::CON_DrawConsole;
				case 0x000C8960: return StretchPicCaller::DrawStretchPic;
				case 0x000DBF10: return StretchPicCaller::loadbox_c_GetIndices;
				case 0x00006DC0: return StretchPicCaller::ScopeCalcXY;

				default: return StretchPicCaller::Unknown;
			}
			break;
		case Module::RefDll:
			switch (fnStartRva) {
				case 0x00002A40: return StretchPicCaller::Draw_Line;
				default: return StretchPicCaller::Unknown;
			}
			break;
		case Module::PlayerDll:
			return StretchPicCaller::Unknown;
		case Module::GameDll:
			return StretchPicCaller::Unknown;
		case Module::SpclDll:
			return StretchPicCaller::Unknown;
		case Module::Unknown:
			return StretchPicCaller::Unknown;
	}
}

inline StretchPicCaller getStretchPicCallerFromRva(uint32_t fnStartRva) { return getStretchPicCallerFrom(moduleForFnStart(fnStartRva), fnStartRva); }

/*
    Draw_Pic
*/
enum class PicCaller {
	Unknown = 0,
	ExecuteLayoutString,
	SCR_DrawCrosshair,
	NetworkDisconnectIcon,
	BackdropDraw,
	DrawPicWrapper
};
inline PicCaller getPicCallerFrom(Module m, uint32_t fnStartRva) {
	switch (m) {
		case Module::SofExe:
			switch (fnStartRva) {
				case 0x00014510: return PicCaller::ExecuteLayoutString;
				case 0x00015DA0: return PicCaller::SCR_DrawCrosshair;
				case 0x000165A0: return PicCaller::NetworkDisconnectIcon;
				case 0x000C8A40: return PicCaller::DrawPicWrapper;
				default: return PicCaller::Unknown;
			}
			break;
		case Module::RefDll:
			return PicCaller::Unknown;
		case Module::PlayerDll:
			return PicCaller::Unknown;
		case Module::GameDll:
			return PicCaller::Unknown;
		case Module::SpclDll:
			return PicCaller::Unknown;
		case Module::Unknown:
			return PicCaller::Unknown;
	}
}
inline PicCaller getPicCallerFromRva(uint32_t fnStartRva) { return getPicCallerFrom(moduleForFnStart(fnStartRva), fnStartRva); }

/*
	Draw_PicOptions
*/
enum class PicOptionsCaller {
	Unknown = 0,
	cInterface_DrawNum,
	cDMRanking_CalcXY,
	SCR_DrawCrosshair,
	spcl_DrawFPS,
};
inline PicOptionsCaller getPicOptionsCallerFrom(Module m, uint32_t fnStartRva) {
	switch (m) {
		case Module::SofExe:
			switch (fnStartRva) {
				case 0x000065E0: return PicOptionsCaller::cInterface_DrawNum;
				case 0x00007AF0: return PicOptionsCaller::cDMRanking_CalcXY;
				case 0x00015DA0: return PicOptionsCaller::SCR_DrawCrosshair;
				default: return PicOptionsCaller::Unknown;
			}
		case Module::SpclDll:
			switch (fnStartRva) {
				case 0x00003420: return PicOptionsCaller::spcl_DrawFPS;
				default: return PicOptionsCaller::Unknown;
			}
			break;
		case Module::RefDll:
			return PicOptionsCaller::Unknown;
		case Module::PlayerDll:
			return PicOptionsCaller::Unknown;
		case Module::GameDll:
			return PicOptionsCaller::Unknown;
		case Module::Unknown:
			return PicOptionsCaller::Unknown;
	}
}
inline PicOptionsCaller getPicOptionsCallerFromRva(uint32_t fnStartRva) { return getPicOptionsCallerFrom(moduleForFnStart(fnStartRva), fnStartRva); }

/*
    Draw_CroppedPicOptions
*/
enum class CroppedPicCaller {
	Unknown = 0,
	cInventory2_Constructor,
};
inline CroppedPicCaller getCroppedPicCallerFrom(Module m, uint32_t fnStartRva) {
	switch (m) {
		case Module::SofExe:
			switch (fnStartRva) {
				case 0x00008260: return CroppedPicCaller::cInventory2_Constructor;
				default: return CroppedPicCaller::Unknown;
			}
			break;
		case Module::RefDll:
			return CroppedPicCaller::Unknown;
		case Module::PlayerDll:
			return CroppedPicCaller::Unknown;
		case Module::GameDll:
			return CroppedPicCaller::Unknown;
		case Module::SpclDll:
			return CroppedPicCaller::Unknown;
		case Module::Unknown:
			return CroppedPicCaller::Unknown;
	}
}
inline CroppedPicCaller getCroppedPicCallerFromRva(uint32_t fnStartRva) { return getCroppedPicCallerFrom(moduleForFnStart(fnStartRva), fnStartRva); }

/*
    GL_FindImage
*/
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
	trITexture_c_load, //traintexture,load
	R_MakeSkyVec
};
inline FindImageCaller getFindImageCallerFrom(Module m, uint32_t fnStartRva) {
	switch (m) {
		case Module::SofExe:
			switch (fnStartRva) {
				default: return FindImageCaller::Unknown;
			}
		case Module::RefDll:
			switch (fnStartRva) {
				case 0x00001850: return FindImageCaller::Draw_Char;
				case 0x00001D10: return FindImageCaller::Draw_StretchPic;
				case 0x00002A40: return FindImageCaller::Draw_Line;
				case 0x00006A90: return FindImageCaller::LoadTexture;
				case 0x00007380: return FindImageCaller::GL_FindImage;
				case 0x000078E0: return FindImageCaller::InitGammaTable;
				case 0x00007BB0: return FindImageCaller::R_ScrollDetail;
				case 0x0000B050: return FindImageCaller::Mod_LoadFaces;
				case 0x0000CAE0: return FindImageCaller::R_DrawAlphaQuad;
				case 0x0000CEE0: return FindImageCaller::R_DrawParticleList;
				case 0x00016B80: return FindImageCaller::R_GetTerrainPolys;
				case 0x00016F40: return FindImageCaller::trITexture_c_load;
				case 0x00019260: return FindImageCaller::R_MakeSkyVec;
				default: return FindImageCaller::Unknown;
			}
			break;
		case Module::PlayerDll:
			return FindImageCaller::Unknown;
		case Module::GameDll:
			return FindImageCaller::Unknown;
		case Module::SpclDll:
			return FindImageCaller::Unknown;
		case Module::Unknown:
			return FindImageCaller::Unknown;
	}
}

inline FindImageCaller getFindImageCallerFromRva(uint32_t fnStartRva) { return getFindImageCallerFrom(moduleForFnStart(fnStartRva), fnStartRva); }

/*
    glVertex2f
*/
enum class VertexCaller {
	Unknown = 0,
	Draw_Char,
	Draw_StretchPic,
	Draw_Line
};
inline VertexCaller getVertexCallerFromRva(Module m, uint32_t fnStartRva) {
	switch (m) {
		case Module::SofExe:
			return VertexCaller::Unknown;
		case Module::RefDll:
			switch (fnStartRva) {
				case 0x00001850: return VertexCaller::Draw_Char;
				case 0x00001D10: return VertexCaller::Draw_StretchPic;
				case 0x00002A40: return VertexCaller::Draw_Line;
				default: return VertexCaller::Unknown;
			}
		case Module::PlayerDll:
			return VertexCaller::Unknown;
		case Module::GameDll:
			return VertexCaller::Unknown;
		case Module::SpclDll:
			return VertexCaller::Unknown;
		case Module::Unknown:
			return VertexCaller::Unknown;
	}
}

/*
	R_DrawFont
*/
enum class FontCaller {
	Unknown = 0,
	ScopeCalcXY,
	DMRankingCalcXY,
	Inventory2,
	SCRDrawPause,
	SCRUpdateScreen,
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
inline FontCaller getFontCallerFrom(Module m, uint32_t fnStartRva) {
	switch (m) {
		case Module::SofExe:
			switch (fnStartRva) {
				case 0x00006DC0: return FontCaller::ScopeCalcXY;
				case 0x00007AF0: return FontCaller::DMRankingCalcXY;
				case 0x00008260: return FontCaller::Inventory2;
				case 0x00013710: return FontCaller::SCRDrawPause;
				case 0x000163C0: return FontCaller::SCRUpdateScreen;
				case 0x000CECC0: return FontCaller::RectDrawTextItem;
				case 0x000CF0E0: return FontCaller::RectDrawTextLine;
				case 0x000D19F0: return FontCaller::TickerDraw;
				case 0x000D4060: return FontCaller::InputHandle;
				case 0x000D9F80: return FontCaller::FileboxHandle;
				case 0x000DBF10: return FontCaller::LoadboxGetIndices;
				case 0x000DE430: return FontCaller::ServerboxDraw;
				case 0x000EA8A0: return FontCaller::TipRender;
				default: return FontCaller::Unknown;
			}
		case Module::RefDll:
			switch (fnStartRva) {
				case 0x00002A40: return FontCaller::DrawLine;
				default: return FontCaller::Unknown;
			}
			break;
		case Module::PlayerDll:
			return FontCaller::Unknown;
		case Module::GameDll:
			return FontCaller::Unknown;
		case Module::SpclDll:
			return FontCaller::Unknown;
		case Module::Unknown:
			return FontCaller::Unknown;
	}
}
inline FontCaller getFontCallerFromRva(uint32_t fnStartRva) { return getFontCallerFrom(moduleForFnStart(fnStartRva), fnStartRva); }

#endif // FEATURE_UI_SCALING
