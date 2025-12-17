#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

void hkcInventory2_And_cGunAmmo2_Draw(void * self, detour_cInventory2_And_cGunAmmo2_Draw::tcInventory2_And_cGunAmmo2_Draw original) {
    SOFBUDDY_ASSERT(self != nullptr);
    SOFBUDDY_ASSERT(original != nullptr);

    g_activeRenderType = uiRenderType::HudInventory;
	original(self);
    g_activeRenderType = uiRenderType::None;

    if (hudInventory_wasItem) {
        hudInventory_wasItem = false;
    } else {
        hudInventory_wasItem = true;
    }
}

#endif // FEATURE_SCALED_HUD

