#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

using detour_cInventory2_And_cGunAmmo2_Draw::ocInventory2_And_cGunAmmo2_Draw;

void __thiscall hkcInventory2_And_cGunAmmo2_Draw(void * self, detour_cInventory2_And_cGunAmmo2_Draw::tcInventory2_And_cGunAmmo2_Draw original) {
	if (!ocInventory2_And_cGunAmmo2_Draw && original) ocInventory2_And_cGunAmmo2_Draw = original;
    //This is the ammo (bottom right) and inventory (bottom left)
    // So positioned from left and bottom edge and right and bottom edge

    g_activeRenderType = uiRenderType::HudInventory;
    ocInventory2_And_cGunAmmo2_Draw(self);
    g_activeRenderType = uiRenderType::None;

    if (hudInventory_wasItem) {
        hudInventory_wasItem = false;
    } else {
        hudInventory_wasItem = true;
    }
}

#endif // FEATURE_SCALED_HUD

