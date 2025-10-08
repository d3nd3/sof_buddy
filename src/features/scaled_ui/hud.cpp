/*
	Scaled UI - HUD Scaling Functions
	
	This file contains all HUD scaling functionality including:
	- HUD element scaling (health, armor, ammo, etc.)
	- Interface element positioning
	- HUD rendering hooks
*/

#include "feature_config.h"

#if FEATURE_UI_SCALING

#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "shared_hook_manager.h"
#include "feature_macro.h"
#include "scaled_ui.h"

#include "DetourXS/detourxs.h"
#include <math.h>
#include <stdint.h>

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
    if (hudDmRanking) {
        float x_scale = static_cast<float>(current_vid_w) / 640.0f;
        int offsetEdge = 40; //36??
        if (hudDmRanking_wasImage) {
            // PrintOut(PRINT_LOG,"Logo ypos = %i\n",y);
            //2nd image, means we in spec chasing someone. This image is a DM Logo.
            x = current_vid_w - 32 * hudScale - offsetEdge * x_scale;
            // y = y + (hudScale-1)*32;
            //go to start of border by removing fixed margin, add previous logo + new scaling margin
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
            //x position is off by 12 pixels, suggesting a ride side bias, not taking pic width into consideration.
            //assume they wanted a 640-584=56 , 56-32 = 24

            //Compare 2 screenshots to see if a larger image gets shifted to the left
            //Ok so it maintains its X position, when it increases in size.

            //When the higher resolution is used, it is calculating the X position as a % from edge.
            //This is fine, it either matches the left edge of the image or the right.
            //It is impossible to match both, else image would get larger/smaller with reso.
            //But by positioning left edge, the right border will appear larger on higher reso.
            //Which is why i preferred to posititon it based on the right edge.

            //Left Edge value = -56, Right Edge value would be -56-32 = -24.

            //Used 48 instead because not enough space in 640x480 otherwise.
            //We could just use 56 again perhaps.

            //Since we are scaling the image.
            //Correct its position

            //We might have to NOP out SCR_DirtyRect calls
            //Or just leave them and do an extra SCR_DirtyRect call.

            // float y_scale = vid_height/480; 
            x = current_vid_w - 32 * hudScale - offsetEdge * x_scale;
            //SOF FORMULA : vid_width - 40 * vid_width/640 - 16 
            w_scale = hudScale;
            h_scale = hudScale;
        }
        // To help R_DrawFont know if teamIcon image was drawn.
        hudDmRanking_wasImage = true;

    } else if (hudInventoryAndAmmo) {
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
}

/*
    This function is called internally by:
    - cInventory2::Draw() (ammo, gun, item displays)
    - cHealthArmour2::Draw() (HP bar, armour bar)
*/
void hkDraw_CroppedPicOptions(int x, int y, int c1x, int c1y, int c2x, int c2y, int palette, char * name) {
    if (hudInventoryAndAmmo) {
        if (!strncmp(name, "pics/interface2/", 16)) {

            if (!strncmp(name + 16, "frame_", 6)) {
                // All used on both left(items) and right(guns).
                if (!strcmp(name + 22, "bottom")) {
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
                    // PrintOut(PRINT_LOG,"frame_top2");
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
    } else if (hudHealthArmor) {
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
    This function is called internally by:
    - cScope::Draw()
    - cCountdown::Draw()
    - cDMRanking::Draw()
    - cInventory2::Draw()
    - cInfoTicker::Draw()
    - cMissionStatus::Draw()
    - SCR_DrawPause()
    - rect_c::DrawTextItem()
    - loadbox_c::Draw()
    - various other menu items
*/
void hkR_DrawFont(int screenX, int screenY, char * text, int colorPalette, char * font, bool rememberLastColor) {
    // static bool wasHudDmRanking = false;
    realFont = getRealFontEnum((char*)(* (int * )(font + 4)));
    
    if (hudDmRanking) {
        static bool scorePhase = true;

        int fontWidth = 12;
        int offsetEdge = 40;

        //Text is only drawn in:
        //Spec Chase Mode
        //Spec OFF 
        float x_scale = static_cast<float>(current_vid_w) / 640.0f;

        if (hudDmRanking_wasImage) {
            //We are SPEC or TEAM DMMODE playing.
            if ( * (int * ) 0x201E7E94 == 7) { //PM_SPECTATOR PM_SPECTATOR_FREEZE
                //PLAYER NAME IN SPEC CHASE MODE.

                // We are in spec chasing someone, so this font is the name of the player we chase.
                // There is an eye and a LOGO above us, if in TEAM DM
                // x = current_vid_w - 32*hudScale - offsetEdge*x_scale;
                //Preserve their centering of text calculation.(Don't want to bother with colorCodes).
                // screenX += 56 * x_scale;
                //40 is a magic number here.
                //It uses 40 as scaling border, and the remaining is 17px.

                //Why is this negative?
                //Oh because it extends to the left, for long names
                int center_offset = screenX - (current_vid_w - (offsetEdge * x_scale));
                // PrintOut(PRINT_LOG,"Center offset is %i\n",center_offset);
                // screenX -= -16 + 32*hudScale - center_offset*(hudScale-1);
                // screenX = current_vid_w - 40 * x_scale - 16 
                //x = width - scaled_border - scaledHud/2 + centered_text
                screenX = current_vid_w - offsetEdge * x_scale - 16 * hudScale + center_offset;
                // y = 20*y_scale + 32*hudScale + 6*y_scale;
                screenY = 20 * screen_y_scale + 64 * hudScale + 6 * screen_y_scale;
            } else {
                // TEAM DM PLAYING - 1 Logo above.
                //Specifically for Team Deathmatch where a logo is shown above score.
                screenX = current_vid_w - 16 * hudScale - offsetEdge * x_scale - hudScale*fontWidth * strlen(text) / 2;
                screenY = screenY + (hudScale - 1) * (32 + 3);

                if ( scorePhase) {
                    scorePhase = false;
                } else 
                {
                    screenY = screenY + (hudScale - 1) * (realFontSizes[realFont] + 3);
                    scorePhase = true;
                }
            }

        } else {
            //PLAYING IN NON-TEAM DM, so NO LOGO
            if ( scorePhase) {
                scorePhase = false;
            } else 
            {
                screenY = screenY + (hudScale - 1) * (realFontSizes[realFont] + 3);
                scorePhase = true;
            }
            screenX = current_vid_w - offsetEdge * x_scale - 16 * hudScale - hudScale*fontWidth * strlen(text) / 2;
        }
        /*
            Score -> 0
            Ranking -> 1/1
        */
        // PrintOut(PRINT_LOG,"%s : %s : %i\n",text,realFont,someFlag);
        // __asm__("int $3");

        // wasHudDmRanking = true;
    }
    else if (hudInventoryAndAmmo) {
        if ( hudInventory_wasItem ) {
            // The error here is a typo: 'test[0]' and 'test[1]' are used instead of 'text[0]' and 'text[1]'.
            // It should be 'text[0]' and 'text[1]' in the condition.
            if (text && ( (text[0] >= '0' && text[0] <= '8') || (text[0] =='9' && text[1] != 'm') ) ) {
                float set_x,set_y;
                enumCroppedDrawMode before = hudCroppedEnum;
                hudCroppedEnum = ITEM_INVEN_BOTTOM;
                //bottom-right
                drawCroppedPicVertex(false,false,set_x,set_y);
                hudCroppedEnum = before;

                screenX = set_x + -52.0f * hudScale;
                screenY = set_y + -29.0f * hudScale;
            } else {
                float set_x,set_y;
                enumCroppedDrawMode before = hudCroppedEnum;
                hudCroppedEnum = ITEM_INVEN_SWITCH;
                set_x = screenY;
                set_y = screenY;
                //top-right
                drawCroppedPicVertex(true,false,set_x,set_y);
                hudCroppedEnum = before;

                screenX = set_x + -111.0f * hudScale;
                screenY = set_y;
            }
            
            
        } else {
            if (text && ( (text[0] >= '0' && text[0] <= '8') || (text[0] =='9' && text[1] != 'M') ) ) {
                float set_x,set_y;
                enumCroppedDrawMode before = hudCroppedEnum;
                hudCroppedEnum = GUN_AMMO_BOTTOM;

                //bottom-right
                drawCroppedPicVertex(false,false,set_x,set_y);
                hudCroppedEnum = before;

                screenX = set_x + -97.0f * hudScale;
                screenY = set_y + -29.0f * hudScale;
                
            }
            else {
                //animation from screenY 433->406->433
                float set_x,set_y;
                enumCroppedDrawMode before = hudCroppedEnum;
                hudCroppedEnum = GUN_AMMO_SWITCH;
                set_x = screenY;
                set_y = screenY;
                //top-right
                drawCroppedPicVertex(true,false,set_x,set_y);
                hudCroppedEnum = before;

                screenX = set_x + -79.0f * hudScale;
                screenY = set_y;
            }
        }
        /*
            if ( !strcmp(text,"18")) {
                __asm__("int $3");
            }
        */
            // PrintOut(PRINT_LOG,"Inventory UI Font: %s\n",text);
    }
    oR_DrawFont(screenX, screenY, text, colorPalette, font, rememberLastColor);

    characterIndex = 0;
}

// HUD interface drawing functions
void __thiscall hkcInventory2_And_cGunAmmo2_Draw(void * self) {
    //This is the ammo (bottom right) and inventory (bottom left)
    // So positioned from left and bottom edge and right and bottom edge

    hudInventoryAndAmmo = true;
    extern void (__thiscall *ocInventory2_And_cGunAmmo2_Draw)(void * self);
    ocInventory2_And_cGunAmmo2_Draw(self);
    hudInventoryAndAmmo = false;

    if (hudInventory_wasItem) {
        hudInventory_wasItem = false;
    } else {
        hudInventory_wasItem = true;
    }
}

void __thiscall hkcHealthArmor2_Draw(void * self) {
    //This is the health and armour bar at bottom
    //positioned from bottom edge of screen, centered horizontally.

    hudHealthArmor = true;
    extern void (__thiscall *ocHealthArmor2_Draw)(void * self);
    ocHealthArmor2_Draw(self);
    hudHealthArmor = false;
}

void __thiscall hkcDMRanking_Draw(void * self) {
    // Top right of screen, your score and rank
    // So relative to top and right edge

    hudDmRanking = true;
    extern void (__thiscall *ocDMRanking_Draw)(void * self);
    ocDMRanking_Draw(self);
    hudDmRanking = false;
    hudDmRanking_wasImage = false;
}

void __thiscall hkcCtfFlag_Draw(void * self) {
    hudStretchPic = true;
    //Whether you are carrying the flag or the flag is missing.
    //Top left of screen, so relative to top and left edge.
    extern void (__thiscall *ocCtfFlag_Draw)(void * self);
    ocCtfFlag_Draw(self);
    hudStretchPic = false;
}

// HUD scaling CVar change callback
void hudscale_change(cvar_t * cvar) {
    //round to nearest quarter
    hudScale = roundf(cvar->value * 4.0f) / 4.0f;
}

#endif // FEATURE_UI_SCALING
