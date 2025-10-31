/*
	Scaled UI - HUD Scaling Functions
	
	Contains:
        hkDraw_PicOptions
            - can position AND scale using parameter to the fn.
            - dmRanking sprites / special "interface" font scaled

        hkDraw_CroppedPicOptions
            - identifies textures from a hud type
            - sets enum from .m32 pathname
            - stores width/height
            - the glVertex specific hook applies the scaling deeper

        hkR_DrawFont
            - realigns/positions HUD text for scaling.
            - eg. dmRanking Names, position
            - ammunition , name of weapon

        hkcInventory2_And_cGunAmmo2_Draw
            - identification of the ammo and gun ui's (2 corners)

        hkcHealthArmor2_Draw
            - identification of the health and armour bar

        hkcDMRanking_Draw
            - identification of the dmRanking sprites

        hkcCtfFlag_Draw
            - identification of the ctf flag being carried sprite

        hkSCR_ExecuteLayoutString
            - identification of the scoreboard
*/

#include "feature_config.h"

#if FEATURE_UI_SCALING

#include <stdint.h>
#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "shared_hook_manager.h"
#include "feature_macro.h"
#include "scaled_ui.h"

#include "hook_callsite.h"

#include <math.h>
#include <string.h>

// HUD scaling function implementations

realFontEnum_t getRealFontEnum(const char* realFont) {
    if (!realFont) return REALFONT_UNKNOWN;
    if (!strcmp(realFont, "title")) return REALFONT_TITLE;
    if (!strcmp(realFont, "small")) return REALFONT_SMALL;
    if (!strcmp(realFont, "medium")) return REALFONT_MEDIUM;
    if (!strcmp(realFont, "interface")) return REALFONT_INTERFACE;
    return REALFONT_UNKNOWN;
}

// HUD scaling function implementations
/*
    This function is called internally by:
    - cInterface::DrawNum()
    - cDMRanking::Draw()
    - SCR_Crosshair()
*/
void hkDraw_PicOptions(int x, int y, float w_scale, float h_scale, int pal, char * name) {
    
    HookCallsite::recordAndGetFnStartExternal("Draw_PicOptions");
    g_activeDrawCall = DrawRoutineType::PicOptions;
    
    {
        uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("Draw_PicOptions");
        if (fnStart) {
            PicOptionsCaller detected = getPicOptionsCallerFromRva(fnStart);
            if (g_currentPicOptionsCaller == PicOptionsCaller::Unknown && detected != PicOptionsCaller::Unknown) {
                g_currentPicOptionsCaller = detected;
            }
        }
    }

    if (g_activeRenderType == uiRenderType::HudDmRanking) {
        float x_scale = static_cast<float>(current_vid_w) / 640.0f;
        int offsetEdge = 40; //36??
        if (hudDmRanking_wasImage) {
            //This is the 2nd image from the top right corner.
            //Below the eye image.

            //2nd image, means we in spec chasing someone. This image is a DM Logo.
            x = current_vid_w - 32 * hudScale - offsetEdge * x_scale;
            // 20px fixed border, 32px image above, 6px spacing to eye, 
            y = 20 * screen_y_scale + 32 * hudScale + 6 * screen_y_scale;

            w_scale = hudScale;
            h_scale = hudScale;
        } else {
            // PrintOut(PRINT_LOG,"Eye ypos = %i\n",y);
            //pics/interface2/eye = 32x32
            // PrintOut(PRINT_LOG,"%s:: x : %i, y : %i\n",name,x,y);
            // PrintOut(PRINT_LOG,"%s:: w : %f, h : %f\n",name,w,h);
            //x = 2384, y = 60 at gl_mode 0 2560x1440
            //x = 584, y = 20 at gl_mode 3 640x480

            //positioned by top left
            
            //we don't change y here, because y already uses 20*scren_y_scale border. 
            // float y_scale = vid_height/480; 
            x = current_vid_w - 32 * hudScale - offsetEdge * x_scale;
            //SOF FORMULA : vid_width - 40 * vid_width/640 - 16 
            //== Fixed 16 pixels border + 40px scaled by screen resolution
            //== 32px image image eats into fixed border.
            //OUR FORMULA :
            //== SPACE_FOR_IMAGE + 40px scaled by screen resolution
            w_scale = hudScale;
            h_scale = hudScale;
        }
        // To help R_DrawFont know if teamIcon image was drawn.
        hudDmRanking_wasImage = true;

    } else if (g_activeRenderType == uiRenderType::HudInventory) {
        static bool secondDigit = false;

        // //Special Interface Font Wooo (also used in SoFPlus cl_showfps 2)
        float x_scale = static_cast < float > (current_vid_w) / 640.0f;

        // //Bottom-Right Corner - frame_bottom
        x = static_cast < float > (current_vid_w) - 13.0f * x_scale - 63.0f * hudScale;
        if (!secondDigit) x += 18.0f * hudScale;
        // if (left) x -= test2->value * hudScale; //width

        y = static_cast < float > (current_vid_h) - 3.0f * screen_y_scale - 49.0f * hudScale;
        // if (top) y -= 16.0f * hudScale; //height

        w_scale = hudScale;
        h_scale = hudScale;

        if (secondDigit) secondDigit = false;
        else secondDigit = true;
    }

    extern void (*oDraw_PicOptions)(int, int, float, float, int, char*);
    oDraw_PicOptions(x, y, w_scale, h_scale, pal, name);

    g_currentPicOptionsCaller = PicOptionsCaller::Unknown;
    g_activeDrawCall = DrawRoutineType::None;
}

/*
    This function is called internally by:
    - cInventory2::Draw() (ammo, gun, item displays)
    - cHealthArmour2::Draw() (HP bar, armour bar)
*/
void hkDraw_CroppedPicOptions(int x, int y, int c1x, int c1y, int c2x, int c2y, int palette, char * name) {
    
    HookCallsite::recordAndGetFnStartExternal("Draw_CroppedPicOptions");
    
    if (g_activeRenderType == uiRenderType::HudInventory) {
        if (!strncmp(name, "pics/interface2/", 16)) {

            //starts with frame_
            if (!strncmp(name + 16, "frame_", 6)) {
                // All used on both left(items) and right(guns).
                if (!strcmp(name + 22, "bottom")) {
                    //pics/interface2/frame_bottom.m32

                    // texture around bottom of default number display
                    // This has icon of item pickup and Fonts for count of ammo and item count.
                    if (hudInventory_wasItem) {
                        //Gun+Ammo
                        hudCroppedEnum = ITEM_INVEN_BOTTOM;
                    } else {
                        //Item
                        hudCroppedEnum = GUN_AMMO_BOTTOM;
                    }
                } else if (!strcmp(name + 22, "top2")) {
                    //pics/interface2/frame_top2.m32

                    //texture around top of default number display
                    if (hudInventory_wasItem) {
                        // PrintOut(PRINT_LOG,"frame_top2 GUN");
                        //Gun+Ammo
                        hudCroppedEnum = ITEM_INVEN_TOP;
                    } else {
                        // PrintOut(PRINT_LOG,"frame_top2 INVEN");
                        //Item
                        hudCroppedEnum = GUN_AMMO_TOP;
                    }
                } else if (!strcmp(name + 22, "top")) {
                    //pics/interface2/frame_top.m32

                    //texture around top of when you switch weapons or items, whilst it shows their naem in text.
                    if (hudInventory_wasItem) {
                        //Gun+Ammo
                        hudCroppedEnum = ITEM_INVEN_SWITCH;
                    } else {
                        //Item
                        hudCroppedEnum = GUN_AMMO_SWITCH;
                    }
                } else if (!strcmp(name + 22, "lip")) {
                    //tiny pixels that appear when you switch weapons or items, whilst it shows thier name in text.
                    if (hudInventory_wasItem) {
                        //Gun+Ammo
                        hudCroppedEnum = ITEM_INVEN_SWITCH_LIP;
                    } else {
                        //Item
                        hudCroppedEnum = GUN_AMMO_SWITCH_LIP;
                    }
                }
            } else if (!strncmp(name + 16, "item_", 5)) {
                //starts with item_
                // cGunAmmo2::DrawDescription() (Icon of Active Item)
                if (!strcmp(name + 21, "c4") || !strcmp(name + 21, "flash") || !strcmp(name + 21, "neuro") || !strcmp(name + 21, "nightvision") ||
                    !strcmp(name + 21, "claymore") || !strcmp(name + 21, "medkit") || !strcmp(name + 21, "h_grenade") ||
                    !strcmp(name + 21, "flag")) {
                    hudCroppedEnum = ITEM_ACTIVE_LOGO;
                }
            }
            //There is 2 veresions of cInterface:DrawNum() that take different paramters and do different things
            //One is a wrapper around the other, just to take in different argument types.
            //It doesnt' target +moveup, this is confusion in IDA, it is just negative offset from it.
            //Really pointing to the no_0 no_1 no_2 no_3 no_4 no_5 etc m32 files in pics/interface2/
        } //pics/interface2
    } else if (g_activeRenderType == uiRenderType::HudHealthArmor) {
        //@ 2560x1440 - TopLeft Corner is position Of image.
        //Cropped: X:1200 Y:1398 C1X:3 C1Y:2 C2X:158 C2Y:28 NAME:pics/interface2/frame_health

        if (!strcmp(name, "pics/interface2/frame_health")) {
            // if ( c2x - c1x == 7 ) 
            // 	PrintOut(PRINT_LOG,"Cropped: X:%i Y:%i C1X:%i C1Y:%i C2X:%i C2Y:%i NAME:%s\n",x,y,c1x,c1y,c2x,c2y,name);
            hudCroppedEnum = HEALTH_FRAME;
        } else if (!strcmp(name, "pics/interface2/health")) {
            hudCroppedEnum = HEALTH;
        } else if (!strcmp(name, "pics/interface2/armor")) {
            hudCroppedEnum = ARMOR;
        } else if (!strcmp(name, "pics/interface2/frame_stealth")) {
            hudCroppedEnum = STEALTH_FRAME;
        }
    }
    
    croppedWidth = c2x - c1x;
    croppedHeight = c2y - c1y;

    //Lets do vertex manipulation
    extern void (*oDraw_CroppedPicOptions)(int, int, int, int, int, int, int, char*);
    oDraw_CroppedPicOptions(x, y, c1x, c1y, c2x, c2y, palette, name);

    //Reset it.- in case we missed one.
    hudCroppedEnum = OTHER_UNKNOWN;
}




/*
    item and ammo HUD interface drawing functions
    Used in:
        hkDraw_CroppedPicOptions() - sub-texture identification
            drawCroppedPicVertex() - rescale

        my_glVertex2f_DrawFont_X() - font specific centering
*/
void __thiscall hkcInventory2_And_cGunAmmo2_Draw(void * self) {
    //This is the ammo (bottom right) and inventory (bottom left)
    // So positioned from left and bottom edge and right and bottom edge

    g_activeRenderType = uiRenderType::HudInventory;
    extern void (__thiscall *ocInventory2_And_cGunAmmo2_Draw)(void * self);
    ocInventory2_And_cGunAmmo2_Draw(self);
    g_activeRenderType = uiRenderType::None;

    if (hudInventory_wasItem) {
        hudInventory_wasItem = false;
    } else {
        hudInventory_wasItem = true;
    }
}

/*
    This is the health and armour bar at bottom, positioned from bottom edge
     of screen, centered horizontally.
    Used in : 
      hkDraw_CroppedPicOptions() - string match texture name for sub-texture identification 
        actual vertex manipulation in drawCroppedPicVertex()     
*/
void __thiscall hkcHealthArmor2_Draw(void * self) {


    g_activeRenderType = uiRenderType::HudHealthArmor;
    extern void (__thiscall *ocHealthArmor2_Draw)(void * self);
    ocHealthArmor2_Draw(self);
    g_activeRenderType = uiRenderType::None;
}

/*
    scale the DMRanking area of HUD
    used in:
      my_glVertex2f_DrawFont_X()
      hkDraw_PicOptions()
      hkR_DrawFont() 
*/
void __thiscall hkcDMRanking_Draw(void * self) {
    // Top right of screen, your score and rank
    // So relative to top and right edge

    g_activeRenderType = uiRenderType::HudDmRanking;
    extern void (__thiscall *ocDMRanking_Draw)(void * self);
    ocDMRanking_Draw(self);
    g_activeRenderType = uiRenderType::None;
    hudDmRanking_wasImage = false;
}

/*
    scale the CTF Flag HUD Element
    used in:
        hkDraw_StretchPic()
*/
void __thiscall hkcCtfFlag_Draw(void * self) {
    g_activeRenderType = uiRenderType::HudCtfFlag;
    extern void (__thiscall *ocCtfFlag_Draw)(void * self);
    ocCtfFlag_Draw(self);
    g_activeRenderType = uiRenderType::None;
}

/*
    DrawingScoreboard?
    used in:
        hkglVertex2f() to scale+center the scoreboard
*/
void hkSCR_ExecuteLayoutString(char * text) {
    g_activeRenderType = uiRenderType::Scoreboard;
    oSCR_ExecuteLayoutString(text);
    g_activeRenderType = uiRenderType::None;
}

void hudscale_change(cvar_t * cvar) {
    //round to nearest quarter
    hudScale = roundf(cvar->value * 4.0f) / 4.0f;
}


#endif // FEATURE_UI_SCALING
