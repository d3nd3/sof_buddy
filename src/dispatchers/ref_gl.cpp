// ref_gl.dll Hook Dispatchers  
// Memory range: 0x300xxxxx (ref_gl.dll)
// Renderer/graphics functions and OpenGL interface

#include "../hdr/feature_config.h"
#include "../hdr/feature_macro.h"
#include "../hdr/util.h"
#include "../hdr/features.h"



// Example: Main scene rendering (placeholder address)  
// REGISTER_HOOK_VOID(RenderScene, 0x140567890, void, __cdecl) {
//     PrintOut(PRINT_LOG, "=== Scene Rendering Dispatch ===\n");
//     
//     // Pre-scene callbacks (setup, culling modifications)
//     DISPATCH_SHARED_HOOK(PreRenderScene);
//     
//     // Call original scene renderer
//     oRenderScene();
//     
//     // Post-scene callbacks (overlays, effects)
//     DISPATCH_SHARED_HOOK(PostRenderScene);
// }


// =============================================================================
// Include all graphics functions that multiple features might need
// =============================================================================



// =============================================================================
// SHARED RENDERING FUNCTIONS
// =============================================================================
// Functions shared between multiple features (scaled_menu, scaled_font, etc.)

#if FEATURE_MENU_SCALING || FEATURE_FONT_SCALING

// Hook registrations for shared rendering functions
REGISTER_HOOK_LEN(DrawStretchPic, 0x30001D10, 5, void, __cdecl, int x, int y, int w, int h, int palette, char * name, int flags);
REGISTER_HOOK_LEN(Draw_Pic, 0x30001ED0, 7, void, __cdecl, int x, int y, char const * imgname, int palette);
REGISTER_HOOK_LEN(GL_FindImage, 0x30007380, 6, void*, __cdecl, char *filename, int imagetype, char mimap, char allowPicmip);

// Shared DrawStretchPic hook - used by both menu and font scaling
void hkDrawStretchPic(int x, int y, int w, int h, int palette, char * name, int flags) {
#if FEATURE_FONT_SCALING
    // Font scaling logic (from scaled_font/hooks.cpp)
    extern bool isFontOuter;
    extern bool isNotFont;
    extern bool hudStretchPic;
    extern bool menuSliderDraw;
    extern bool menuVerticalScrollDraw;
    extern bool menuLoadboxDraw;
    extern float draw_con_frac;
    extern float hudScale;
    extern int current_vid_h;
    extern void (*orig_SRC_AddDirtyPoint)(int, int);
    
    if (isFontOuter) {
        // Console background scaling
        isNotFont = true;
        int consoleHeight = draw_con_frac * current_vid_h;
        y = -1 * current_vid_h + consoleHeight;
        oDrawStretchPic(x, y, w, h, palette, name, flags);
        orig_SRC_AddDirtyPoint(0, 0);
        orig_SRC_AddDirtyPoint(w - 1, consoleHeight - 1);
        isNotFont = false;
        return;
    }
    
    if (hudStretchPic) {
        // HUD scaling (CTF flag, etc.)
        w = w * hudScale;
        h = h * hudScale;
    } else if (menuSliderDraw) {
        // Menu slider scaling (currently disabled)
        // Logic would go here if needed
    } else if (menuVerticalScrollDraw) {
        // Menu vertical scroll scaling (currently disabled)
        // Logic would go here if needed
    } else if (menuLoadboxDraw) {
        // Menu loadbox scaling (currently disabled)
        // Logic would go here if needed
    }
#endif

#if FEATURE_MENU_SCALING
    // Menu scaling logic would go here if needed
    // Currently menu scaling doesn't modify DrawStretchPic
#endif

    // Default behavior - call original function
    oDrawStretchPic(x, y, w, h, palette, name, flags);
}

// Shared Draw_Pic hook - used by menu scaling
void hkDraw_Pic(int x, int y, char const * imgname, int palette) {
#if FEATURE_MENU_SCALING
    // Menu scaling logic (from scaled_menu/hooks.cpp)
    extern bool isDrawPicTiled;
    extern bool isDrawPicCenter;
    
    if (imgname != nullptr) {
        // Check for backdrop images that should be tiled
        extern const char* get_nth_entry(const char* str, int n);
        const char* thirdEntry = get_nth_entry(imgname, 2); 
        if (thirdEntry) {
            const char* end = thirdEntry;
            while (*end && *end != '/') ++end;
            size_t len = end - thirdEntry;
            if (len == 8 && strncmp(thirdEntry, "backdrop", 8) == 0) {
                isDrawPicTiled = true;
                oDraw_Pic(x, y, imgname, palette);
                isDrawPicTiled = false;
                return;
            }
        }
    }

    isDrawPicCenter = true;
    oDraw_Pic(x, y, imgname, palette);
    isDrawPicCenter = false;
#else
    // Default behavior - call original function
    oDraw_Pic(x, y, imgname, palette);
#endif
}

// Shared GL_FindImage hook - used by menu scaling
void* hkGL_FindImage(char *filename, int imagetype, char mimap, char allowPicmip) {
    void * image_t = oGL_FindImage(filename, imagetype, mimap, allowPicmip);
    
#if FEATURE_MENU_SCALING
    if (image_t) {
        extern bool isDrawPicCenter;
        extern bool isDrawPicTiled;
        extern int DrawPicWidth;
        extern int DrawPicHeight;
        extern float screen_y_scale;
        
        if (isDrawPicCenter) {
            DrawPicWidth = *(short*)((char*)image_t + 0x44);
            DrawPicHeight = *(short*)((char*)image_t + 0x46);
        } else if (isDrawPicTiled) {
            DrawPicWidth = *(short*)((char*)image_t + 0x44) * screen_y_scale;
            DrawPicHeight = *(short*)((char*)image_t + 0x46) * screen_y_scale;
        }
    }
#endif
    
    return image_t;
}

#endif // FEATURE_MENU_SCALING || FEATURE_FONT_SCALING
