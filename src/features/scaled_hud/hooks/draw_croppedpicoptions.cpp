#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"
#include "debug/hook_callsite.h"
#include <string.h>

void hkDraw_CroppedPicOptions(int x, int y, int c1x, int c1y, int c2x, int c2y, int palette, char * name, detour_Draw_CroppedPicOptions::tDraw_CroppedPicOptions original) {
    
    HookCallsite::recordAndGetFnStartExternal("Draw_CroppedPicOptions");
    
    if (g_activeRenderType == uiRenderType::HudInventory) {
        if (!strncmp(name, "pics/interface2/", 16)) {

            //starts with frame_
            if (!strncmp(name + 16, "frame_", 6)) {
                // All used on both left(items) and right(guns).
                if (!strcmp(name + 22, "bottom")) {
                    if (hudInventory_wasItem) {
                        //Gun+Ammo
                        hudCroppedEnum = ITEM_INVEN_BOTTOM;
                    } else {
                        //Item
                        hudCroppedEnum = GUN_AMMO_BOTTOM;
                    }
                } else if (!strcmp(name + 22, "top2")) {
                    if (hudInventory_wasItem) {
                        //Gun+Ammo
                        hudCroppedEnum = ITEM_INVEN_TOP;
                    } else {
                        //Item
                        hudCroppedEnum = GUN_AMMO_TOP;
                    }
                } else if (!strcmp(name + 22, "top")) {
                    if (hudInventory_wasItem) {
                        //Gun+Ammo
                        hudCroppedEnum = ITEM_INVEN_SWITCH;
                    } else {
                        //Item
                        hudCroppedEnum = GUN_AMMO_SWITCH;
                    }
                } else if (!strcmp(name + 22, "lip")) {
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
                if (!strcmp(name + 21, "c4") || !strcmp(name + 21, "flash") || !strcmp(name + 21, "neuro") || !strcmp(name + 21, "nightvision") ||
                    !strcmp(name + 21, "claymore") || !strcmp(name + 21, "medkit") || !strcmp(name + 21, "h_grenade") ||
                    !strcmp(name + 21, "flag")) {
                    hudCroppedEnum = ITEM_ACTIVE_LOGO;
                }
            }
        } //pics/interface2
    } else if (g_activeRenderType == uiRenderType::HudHealthArmor) {
        if (!strcmp(name, "pics/interface2/frame_health")) {
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
    original(x, y, c1x, c1y, c2x, c2y, palette, name);

    //Reset it.- in case we missed one.
    hudCroppedEnum = OTHER_UNKNOWN;
}

#endif // FEATURE_SCALED_HUD

