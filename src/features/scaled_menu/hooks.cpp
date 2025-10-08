#include "../../../hdr/feature_config.h"
#if FEATURE_MENU_SCALING

#include "../../../hdr/sof_compat.h"
#include "../../../hdr/features.h"
#include "../../../hdr/util.h"
#include "../../../hdr/shared_hook_manager.h"
#include "../../../hdr/feature_macro.h"

#include "DetourXS/detourxs.h"

#include <math.h>
#include <stdint.h>

// Lifecycle callback registration placed at the top for visibility
static void scaledMenu_EarlyStartup(void);
static void scaledMenu_RefDllLoaded(void);

REGISTER_SHARED_HOOK_CALLBACK(EarlyStartup, scaled_menu, scaledMenu_EarlyStartup, 80, Post);
REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, scaled_menu, scaledMenu_RefDllLoaded, 70, Post);

// Hook registrations for menu scaling
REGISTER_HOOK(M_PushMenu, 0x200C7630, void, __cdecl, const char* name, const char* frame, bool force);

// Note: Draw_Pic, GL_FindImage, and DrawStretchPic hooks are now in dispatchers/ref_gl.cpp
REGISTER_HOOK_LEN(R_Strlen, 0x300042F0, 7, int, __cdecl, char * str, char * fontStd);
REGISTER_HOOK_LEN(R_StrHeight, 0x300044C0, 7, int, __cdecl, char * fontStd);
REGISTER_HOOK_LEN(stm_c_ParseStm, 0x200E7E70, 5, char*, __thiscall, void *self_stm_c, void * toke_c);

// Function pointers for original functions
void (__thiscall *orig_slider_c_Draw)(void * self);
void (__thiscall *orig_loadbox_c_Draw)(void * self);
void (__thiscall *orig_vbar_c_Draw)(void * self);
void (__thiscall *orig_master_Draw)(void * rect_c_self);
void (__thiscall *orig_frame_c_Constructor)(void* self, void * menu_c, char * width, char * height, void * frame_name) = (void(__thiscall*)(void*, void*, char*, char*, void*))0x200C60A0;
// orig_stm_c_ParseStm is now handled by REGISTER_HOOK_LEN macro
void (__thiscall *orig_stm_c_ParseBlank)(void *self_stm_c, void * toke_c);
int (__thiscall *orig_toke_c_GetNTokens)(void * toke_c, int quantity) = (int(__thiscall*)(void*, int))0x200EAFE0;
char * (__thiscall *orig_toke_c_Token)(void * toke_c, int idx) = (char*(__thiscall*)(void*, int))0x200EB440;
void * (__cdecl *orig_new_std_string)(int length) = (void*(__cdecl*)(int))0x200FA352;


// Legacy function pointers (keeping for compatibility)
void (*orig_DrawGetPicSize)(void * stm_c, int *, int *, char * stdPicName);
void (*orig_Draw_GetPicSize)(int *w, int *h, char *pic) = NULL;

// Global variables for menu scaling
bool isDrawPicTiled = false;
bool isDrawPicCenter = false;
int DrawPicWidth = 0;
int DrawPicHeight = 0;
bool isMenuSpmSettings = false;

// Forward declarations
// Note: hkDraw_Pic and hkGL_FindImage are now in dispatchers/ref_gl.cpp
void my_Draw_GetPicSize(int *w, int *h, char *pic);
int hkR_Strlen(char * str, char * fontStd);
int hkR_StrHeight(char * fontStd);
void my_M_PushMenu(const char * name, const char * frame, bool force);
void hkM_PushMenu(const char * name, const char * frame, bool force);
char * findClosingBracket(char * start);
void mutateWidthTokeC_resize(void * toke_c);
void mutateWidthTokeC_width_height(void * toke_c, char * match);
void mutateBlankTokeC_width_height(void * toke_c);
void __thiscall my_master_Draw(void * self);
void __thiscall my_slider_c_Draw(void * self);
void __thiscall my_loadbox_c_Draw(void * self);
void __thiscall my_vbar_c_Draw(void * self);
void __thiscall my_frame_c_Constructor(void* self, void * menu_c, char * width, char * height, void * frame_name);
char * __thiscall hkstm_c_ParseStm(void *self_stm_c, void * toke_c);
char * __thiscall my_rect_c_Parse(void* toke_c, int idx);
void __thiscall my_stm_c_ParseBlank(void *self_stm_c, void * toke_c);

// Note: hkDraw_Pic and hkGL_FindImage functions moved to dispatchers/ref_gl.cpp

/*
  For some menu images. Except icons directory.
*/
void my_Draw_GetPicSize(int *w, int *h, char *pic)
{
  // Split the pic string by '/' and check if the 3rd entry is "icons"
  if (pic != nullptr) {
    // 0-based: 2 is the 3rd entry
    const char* thirdEntry = get_nth_entry(pic, 2); 
    if (thirdEntry) {
      // Find the end of the third entry
      const char* end = thirdEntry;
      while (*end && *end != '/') ++end;
      size_t len = end - thirdEntry;
      if ((len == 5 && strncmp(thirdEntry, "icons", 5) == 0) || ( len == 9 && strncmp(thirdEntry, "teamicons", 9) == 0) ) {
        orig_Draw_GetPicSize (w, h, pic);
        return;
      } else if (len == 8 && strncmp(thirdEntry, "backdrop", 8) == 0) {
        /*
          This idea is good!, except the tiling system is forcing chunks of 128 it seems?
          So if doesn't fully divide by 128, it gives nearest, leaving empty space.
        */
        orig_Draw_GetPicSize (w , h, pic);
        *w = *w * screen_y_scale;
        *h = *h * screen_y_scale;
        return;
      } else if (len == 4 && strncmp(thirdEntry, "misc", 4) == 0) {
        // Check if the last 4 characters of the path are "vend"
        size_t picLen = strlen(pic);
        if (picLen >= 4 && strncmp(pic + picLen - 4, "vend", 4) == 0) {
          orig_Draw_GetPicSize(w, h, pic);
          // *w = *w * screen_y_scale;
          // *h = *h * screen_y_scale;
          return;
        }
      }
    }
  }
  // orig_Com_Printf("pic %s\n",pic);
  orig_Draw_GetPicSize (w, h, pic);
  #if 1
  *w = *w * screen_y_scale;
  *h = *h * screen_y_scale;
  #endif
}
 
//Returns endpointer that we interested in for options
char * findClosingBracket(char * start) {
    char * ret = start;
    
    while ( *ret != 0x00 && *ret != '>' )
    {
        ret++;
    }
    
    return ret;
}

void mutateWidthTokeC_resize(void * toke_c) {
    // Update the existing string structure to point to new data
    char** start_ptr = (char**)((char*)toke_c + 0);
    char** cursor_ptr = (char**)((char*)toke_c + 4);  // _DataPointer at +4
    
    //The index after the null character
    char** end_ptr = (char**)((char*)toke_c + 8);  // offsetEnd at +8
    
    char** unknown_one = (char**)((char*)toke_c + 0xC);
    
    //This capacity includes null terminator.
    size_t* length_ptr = (size_t*)((char*)toke_c + 0x10);  // Length at +10
    
    int preserve_cursor = *cursor_ptr - *start_ptr;
    
    //equal, yet when I do that, I see bug.
    // orig_Com_Printf("%i vs %i\n", *length_ptr, strlen(*start_ptr)+1);
    
    bool changed = false;
    int i = 0;
    std::string out;
    
    
    while (i < *length_ptr-1) {
        const char * input = *start_ptr;
        if (i + 7 < *length_ptr && strncmp(&input[i], "resize ", 7) == 0) {
            // Tentative starting point for parsing after "resize "
            const char* parse_ptr = &input[i + 7];
            char* end_ptr1;
            
            // Parse the first integer
            long val1 = strtol(parse_ptr, &end_ptr1, 10);
            
            // Check if parsing failed or if there's no space after the first number
            if (end_ptr1 == parse_ptr || *end_ptr1 != ' ') {
                out.push_back(input[i]);
                ++i;
                continue;
            }
            if ( val1 > 0 ) val1 = val1 * screen_y_scale;
            // Move past the first number and the space
            parse_ptr = end_ptr1 + 1;
            char* end_ptr2;
            
            // Parse the second integer
            long val2 = strtol(parse_ptr, &end_ptr2, 10);
            
            // Check if parsing failed or if there's no closing '>'
            if (end_ptr2 == parse_ptr) {
                // orig_Com_Printf("Not VALID PATTERN2\n");
                out.push_back(input[i]);
                ++i;
                continue;
            }
            if ( val2 > 0 ) val2 = val2 * screen_y_scale;
            // --- SUCCESS: The entire "resize <int> <int>" pattern was matched ---
            orig_Com_Printf("Mutated resize\n");
            // Build the replacement string
            char buf[128]; // Increased buffer size for safety with large numbers
            int n = snprintf(buf, sizeof(buf), "resize %ld %ld", 
            val1, 
            val2);
            out.append(buf, n);
            
            changed = true;
            
            // Advance i past the entire matched pattern
            i = end_ptr2 - input;
            
        } else {
            // The string at the current position doesn't start with "resize ",
            // so just copy the character and move on.
            out.push_back(input[i]);
            ++i;
        }
    }
    
    int out_len = out.length();
    
    
    char * new_string_data = (char*)orig_new_std_string(out_len + 1);
    if (new_string_data == NULL) {
        // orig_Com_Printf("ERROR: allocation failed!\n");
        return;
    }
    
    memcpy(new_string_data,out.c_str(),out_len+1);
    new_string_data[out_len] = 0x00;
    
    #if 0
    if ( printSuccess )  {
        orig_Com_Printf("BEFORE!\n");
        for ( int k = 0 ; k < *length_ptr; k++ ) {
            char p = (*start_ptr)[k];
            if ( p != 0xD )
            orig_Com_Printf("%c",p);
        }
        orig_Com_Printf("\n");
        
        orig_Com_Printf("AFTER!\n");
        for ( int k = 0 ; k < out_len+1; k++ ) {
            if ( new_string_data[k] != 0xD )
            orig_Com_Printf("%c",new_string_data[k]);
        }
        orig_Com_Printf("\n");
    }
    #endif
    
    // Free the old string data
    if (*start_ptr != NULL) {
        static void (*Z_Free)(void *pvAddress) = (void(*)(void*))0x200F9D32;
        Z_Free(*start_ptr);
    }
    
    // Update the structure to point to new data
    *start_ptr = new_string_data;
    *cursor_ptr = new_string_data + preserve_cursor;
    //  *unknown_one = new_string_data;
    *end_ptr = new_string_data + out_len + 1;
    //If I use out_len +1 i see weird character on screen :/
    *length_ptr = out_len+1; //includes null terminator
    
    // orig_Com_Printf("new length = %i\n", *length_ptr);
}

void mutateWidthTokeC_width_height(void * toke_c, char * match) {
    // Update the existing string structure to point to new data
    char** start_ptr = (char**)((char*)toke_c + 0);
    char** cursor_ptr = (char**)((char*)toke_c + 4);  // _DataPointer at +4
    
    //The index after the null character
    char** end_ptr = (char**)((char*)toke_c + 8);  // offsetEnd at +8
    
    char** unknown_one = (char**)((char*)toke_c + 0xC);
    
    //This capacity includes null terminator.
    size_t* length_ptr = (size_t*)((char*)toke_c + 0x10);  // Length at +10
    
    int preserve_cursor = *cursor_ptr - *start_ptr;
    
    //equal, yet when I do that, I see bug.
    // orig_Com_Printf("%i vs %i\n", *length_ptr, strlen(*start_ptr)+1);
    
    
    
    bool match_within_cursor = false;
    const char *input = *start_ptr;
    int match_len = strlen(match);
    
    char * endOfOptions = findClosingBracket(*cursor_ptr);
    
    #if 0
    orig_Com_Printf("\nStart...\n");
    for ( int k = 0 ; k < endOfOptions - *cursor_ptr; k++ ) {
        char p = (*cursor_ptr)[k];
        if ( p != 0xD )
        orig_Com_Printf("%c",p);
    }
    orig_Com_Printf("\nEnd...\n");
    
    #endif
    // Scan from cursor to end for the match
    for (char * ci = *cursor_ptr; ci + match_len <= endOfOptions; ++ci) {
        if (strncmp(ci, match, match_len) == 0) {
            match_within_cursor = true;
            break;
        }
    }
    if (!match_within_cursor) {
        // No match from cursor to end, so do not mutate
        return;
    }
    
    bool changed = false;
    
    std::string out;
    
    
    
    // Fill 'out' with the part from *start_ptr up to (but not including) *cursor_ptr
    if (*cursor_ptr > *start_ptr) {
        out.append(*start_ptr, *cursor_ptr - *start_ptr);
    }
    
    int i = preserve_cursor;
    while (i < endOfOptions - *start_ptr) {
        const char * input = *start_ptr;
        if (i + match_len - 1 < *length_ptr - 1 && strncmp(&input[i], match, match_len) == 0) {
            // Tentative starting point for parsing after match
            const char* parse_ptr = &input[i + match_len];
            char* end_ptr1;
            
            // Parse the integer after the match
            long val1 = strtol(parse_ptr, &end_ptr1, 10);
            
            // Check if parsing failed
            if (end_ptr1 == parse_ptr) {
                out.push_back(input[i]);
                ++i;
                continue;
            }
            if (val1 > 0) val1 = val1 * screen_y_scale;
            
            orig_Com_Printf("Mutated %s\n", match);
            
            // Build the replacement string
            char buf[128];
            int n = snprintf(buf, sizeof(buf), "%s%ld", match, val1);
            out.append(buf, n);
            
            changed = true;
            
            // Advance i past the matched pattern and the number
            i = end_ptr1 - input;
        } else {
            out.push_back(input[i]);
            ++i;
        }
    }
    
    // Append the rest of the string after endOfOptions, if any
    // if endOfOptions is to the left of the nullCharacter.
    if (endOfOptions < *start_ptr + *length_ptr - 1) {
        //append characters up to but not including the null char.
        out.append(endOfOptions, (*start_ptr + *length_ptr - 1) - endOfOptions);
    }
    
    
    int out_len = out.length();
    
    
    char * new_string_data = (char*)orig_new_std_string(out_len + 1);
    if (new_string_data == NULL) {
        // orig_Com_Printf("ERROR: allocation failed!\n");
        return;
    }
    
    memcpy(new_string_data,out.c_str(),out_len+1);
    new_string_data[out_len] = 0x00;
    
    
    // Free the old string data
    if (*start_ptr != NULL) {
        static void (*Z_Free)(void *pvAddress) = (void(*)(void*))0x200F9D32;
        Z_Free(*start_ptr);
    }
    
    int tmp_len = endOfOptions - *cursor_ptr;
    
    // Update the structure to point to new data
    *start_ptr = new_string_data;
    *cursor_ptr = new_string_data + preserve_cursor;
    //  *unknown_one = new_string_data;
    *end_ptr = new_string_data + out_len + 1;
    //If I use out_len +1 i see weird character on screen :/
    *length_ptr = out_len+1; //includes null terminator
    
    #if 0
    orig_Com_Printf("\nAFTERStart...\n");
    for ( int k = 0 ; k < tmp_len; k++ ) {
        char p = (*cursor_ptr)[k];
        if ( p != 0xD )
        orig_Com_Printf("%c",p);
    }
    orig_Com_Printf("\nAFTEREnd...\n");
    #endif
    
    // orig_Com_Printf("new length = %i\n", *length_ptr);
}

/*
 blank <width> <height>
 When a page is loaded into a frame, the blank width is naturally rescaled to be a % of the frame's
 width. "Strange".

 But with larger font, the offset is too small.
*/
void mutateBlankTokeC_width_height(void * toke_c) {
    // Access all relevant structure members, including the cursor
    char** start_ptr = (char**)((char*)toke_c + 0);
    char** cursor_ptr = (char**)((char*)toke_c + 4);
    char** end_ptr = (char**)((char*)toke_c + 8);
    size_t* length_ptr = (size_t*)((char*)toke_c + 0x10);

    // CRITICAL FIX: Preserve the cursor's relative position
    int preserve_cursor = *cursor_ptr - *start_ptr;

    const char* input = *start_ptr;
    std::string out;
    int i = 0;
    int len = *length_ptr - 1; // String length without null terminator
    bool changed = false;

    // Use a robust, non-incremental parsing strategy inside the loop
    while (i < len) {
        // Check for the full prefix "<blank " to be more specific and avoid partial matches
        if (i + 7 < len && strncmp(&input[i], "<blank ", 7) == 0) {
            const char* parse_ptr = &input[i + 7]; // Start parsing after "<blank "
            char* end_width;

            // Parse width
            long width = strtol(parse_ptr, &end_width, 10);

            // ROBUST CHECK: Ensure a number was parsed AND it's followed by a space
            if (end_width == parse_ptr || *end_width != ' ') {
                // Pattern is not `<blank <int> <int>`, treat as normal text
                out.push_back(input[i]);
                ++i;
                continue;
            }

            // Advance parser past the width and the separating space
            parse_ptr = end_width + 1;
            char* end_height;

            // Parse height
            long height = strtol(parse_ptr, &end_height, 10);

            // ROBUST CHECK: Ensure a number was parsed
            if (end_height == parse_ptr) {
                // Pattern is not `<blank <int> <int>`, treat as normal text
                out.push_back(input[i]);
                ++i;
                continue;
            }

            // --- SUCCESS: Full pattern matched ---
                        // Apply the scaling logic
            if (width > 0) width = width * screen_y_scale;
            // if (height > 0) height = height * screen_y_scale;

            // Use snprintf for safe and efficient string construction
            char buf[128]; // Buffer for the new formatted string
            int n = snprintf(buf, sizeof(buf), "<blank %ld %ld", width, height);
            out.append(buf, n);
            orig_Com_Printf("Rescaling a BLANK width hight to %s\n",buf);
            changed = true;

            // Advance the main loop counter past the entire matched pattern
            i = end_height - input;

        } else {
            // Not the target pattern, just copy the character
            out.push_back(input[i]);
            ++i;
        }
    }

    int out_len = out.length();
    char* new_string_data = (char*)orig_new_std_string(out_len + 1);
    if (new_string_data == NULL) {
        return; // Allocation failed
    }
    memcpy(new_string_data, out.c_str(), out_len + 1);

    // Free the old string data
    if (*start_ptr != NULL) {
        static void (*Z_Free)(void *pvAddress) = (void (*)(void *))0x200F9D32;
        Z_Free(*start_ptr);
    }

    // Update all structure pointers to point to the new data
    *start_ptr = new_string_data;
    
    // CRITICAL FIX: Restore the cursor position in the new string
    if (preserve_cursor > out_len) preserve_cursor = out_len; // Safety clamp
    *cursor_ptr = new_string_data + preserve_cursor;
    
    *end_ptr = new_string_data + out_len + 1;
    *length_ptr = out_len + 1; // Length includes the null terminator
}

//stm_c::Render(void)
void __thiscall my_master_Draw(void * self)
{
    //ecx = *ecx
    //call *(ecx+0x1C)
    
    // Fix: Avoid arithmetic on void pointer and use correct casting
    void **vtable = *(void ***)self;
    void (__thiscall *func)(void *) = (void (__thiscall *)(void *))vtable[0x1C / sizeof(void *)];
    
    // orig_Com_Printf("vtable : %08X\n",vtable);
    #if 0
    if ( vtable != (void*)0x20112A40 ) {
        //text_c vtable
    } 
    #endif
    func(self);
    
}

/*
Repositions the slider with DrawStretchPic
We were double scaling this, because Draw_GetPicSize is being used for this. duh.
*/
void __thiscall my_slider_c_Draw(void * self) {
    menuSliderDraw = true;
    orig_slider_c_Draw(self);
    menuSliderDraw = false;
}

/*
Was going to try and scale the loadGame screenshots,
but seems too hard atm. Disabled
*/
void __thiscall my_loadbox_c_Draw(void * self) {
    menuLoadboxDraw = true;
    orig_loadbox_c_Draw(self);
    menuLoadboxDraw = false;
}
/*
Disabled currently, but enables custom control of this object.
*/
void __thiscall my_vbar_c_Draw(void * self) {
    menuVerticalScrollDraw = true;
    orig_vbar_c_Draw(self);
    menuVerticalScrollDraw = false;
}

void __thiscall my_frame_c_Constructor(void* self, void * menu_c, char * width, char * height, void * frame_name)
{
    // orig_frame_c_Constructor(self,menu_c,width,height,frame_name);
    // return;
    #if 0
    // Hex dump 64 bytes of the menu_c pointer for debugging, with ASCII output
{
    unsigned char *p = (unsigned char *)menu_c;
    char hexbuf[3*16+1];
    char asciibuf[17];
    orig_Com_Printf("menu_c @ %p hex dump (first 64 bytes):\n", menu_c);
    for (int i = 0; i < 64; i += 16) {
        int hexpos = 0;
        int asciipos = 0;
        for (int j = 0; j < 16; ++j) {
            if (i + j < 64) {
                unsigned char byte = p[i + j];
                snprintf(&hexbuf[hexpos], 4, "%02X ", byte);
                hexpos += 3;
                asciibuf[asciipos++] = (byte >= 32 && byte <= 126) ? byte : '.';
            } else {
                snprintf(&hexbuf[hexpos], 4, "   ");
                hexpos += 3;
                asciibuf[asciipos++] = ' ';
            }
        }
        hexbuf[hexpos] = '\0';
        asciibuf[asciipos] = '\0';
        orig_Com_Printf("%04X: %s |%s|\n", i, hexbuf, asciibuf);
    }
}
#endif

    orig_Com_Printf("%s : %s x %s\n",frame_name, width,height);

    #if 0
    // Disabling stretched menu's becuase we handle it ourselves.
    // If name ends with _ss, replace with _ns
    char name_buf[256];

    
    size_t name_len = strlen(frame_name);
    if (name_len >= 3 && strcmp(frame_name + name_len - 3, "_ss") == 0) {
        // Only modify if it's not a string literal (i.e., not in read-only memory)
        // If you are sure it's always mutable, you can do:
        char *mutable_name = (char *)frame_name;
        mutable_name[name_len - 3] = '_';
        mutable_name[name_len - 2] = 'n';
        mutable_name[name_len - 1] = 's';

    }

    #endif

    //Only scale certain pages that don't break with scaling.
    if ( ( !strcmp(width,"400") && !strcmp(height,"240") ) || 
    isMenuSpmSettings ) {
        orig_Com_Printf("==SCALING FRAME %s==\n",frame_name);
        //if width height specified, we scale it.

        int scaled_width = 0, scaled_height = 0;
        bool width_is_num = false, height_is_num = false;
        
        /*
            Parse the width and height (they are in string format)
        */
        if (width) {
            char *endptr;
            long w = strtol(width, &endptr, 10);
            if (endptr != width && *endptr == '\0') {
                width_is_num = true;
                if (w != 0) {
                    scaled_width = (int)(w * screen_y_scale);
                } else {
                    scaled_width = 0;
                }
            }
        }
        if (height) {
            char *endptr;
            long h = strtol(height, &endptr, 10);
            if (endptr != height && *endptr == '\0') {
                height_is_num = true;
                if (h != 0) {
                    scaled_height = (int)(h * screen_y_scale);
                } else {
                    scaled_height = 0;
                }
            }
        }
        
        char width_buf[32], height_buf[32];
        char *final_width = width, *final_height = height;
        if (width_is_num) {
            snprintf(width_buf, sizeof(width_buf), "%d", scaled_width);
            final_width = width_buf;
        }
        if (height_is_num) {
            snprintf(height_buf, sizeof(height_buf), "%d", scaled_height);
            final_height = height_buf;
        }
        
        
        orig_frame_c_Constructor(self,menu_c,final_width,final_height,frame_name);
    } else { 
        orig_frame_c_Constructor(self,menu_c,width,height,frame_name);
    }
}

char * __thiscall hkstm_c_ParseStm(void *self_stm_c, void * toke_c)
{
    
    
    mutateWidthTokeC_resize(toke_c);

    //find blank
    mutateBlankTokeC_width_height(toke_c);

    #if 0
    int out_len = *(int*)(toke_c+0x10);
    char * toke_data = *(char**)toke_c;
    orig_Com_Printf("AFTER!\n");
    for ( int k = 0 ; k < out_len; k++ ) {
        if ( toke_data[k] != 0xD )
        orig_Com_Printf("%c",toke_data[k]);
    }
    orig_Com_Printf("\n");

    #endif
    char * out_ret = ostm_c_ParseStm(self_stm_c, toke_c);
    
    return out_ret;
    
}

/*
This is hooked on the toke_c::Token(int) call
at the top of rect_c::Parse(toke_c *)
Because hooks dont' seem to work nicely with that weird c++ stuff at top.(FreeMemory etc)

This is called _VERY_ often, for every sub-token it seems.
I mean if there is 'tip <> <>' in options
or 'key <> <>'
They are also rects, and so it creates another rect for these ones.
*/

char * __thiscall my_rect_c_Parse(void* toke_c, int idx)
{
    static char * (__thiscall *orig_call)(void* toke_c, int idx) = (char*(__thiscall*)(void*, int))0x200EB440;
    
    
    mutateWidthTokeC_width_height(toke_c, (char*)"width ");
    mutateWidthTokeC_width_height(toke_c, (char*)"height ");
    
    return orig_call(toke_c, idx);
}

void __thiscall my_stm_c_ParseBlank(void *self_stm_c, void * toke_c)
{
    mutateBlankTokeC_width_height(toke_c);
}

/*

Menu text is positioned and scaled in my_glVertex2f_DrawFont_1
search characterIndex

This is hooked to help resize the rects which contain the text.

===MAIN MENU FONT SCALING==

Somehow <center> tag is not performing under scale.

Seems it applies 5px for a space? (Confirmed.)
We remove the space from this calculation to improve centering.
*/
int hkR_Strlen(char * str, char * fontStd)
{
  // INSERT_YOUR_CODE
  int space_count = 0;
  for (char *p = str; *p != '\0'; ++p) {
    if (*p == ' ') {
      space_count++;
    }
  }
  
  
  #if 0
  if ( !strcmp(str,"EXIT GAME") ) {
    realFontEnum_t fontType = getRealFontEnum((char*)(* (int * )(fontStd + 4)));
    char * teststr = "EXIT GAME";
    int len = oR_Strlen(teststr, fontStd) * hudScale;
    orig_Com_Printf("Len = %i %i %i %i\n",len , space_count, fontType, realFontSizes[fontType]);
    return len;
  }
  #endif

  return screen_y_scale * ( oR_Strlen(str, fontStd) - 5 * space_count);
}

int hkR_StrHeight(char * fontStd)
{
  // return orig_R_StrHeight(fontStd);
  return screen_y_scale * oR_StrHeight(fontStd);
  // return test2->value;
}

/*
[frame] is often ""
menu <name> [frame]
2nd argument to menu is rarely used
menu players/$menu_temp_skin text
*/
void my_M_PushMenu(const char * name, const char * frame, bool force)
{
    
    orig_Com_Printf("M_PushMenu : %s %s %i\n",name,frame, force);
    if ( !strcmp( name, "spm" ) || !strncmp(name,"spm_",4) ) {
        orig_Com_Printf("Detected sofplus settings menu!\n");
        isMenuSpmSettings = true;
        oM_PushMenu(name,frame,force);
        isMenuSpmSettings = false;
        return;
    }
    oM_PushMenu(name,frame,force);
}

// Hook function for M_PushMenu
void hkM_PushMenu(const char * name, const char * frame, bool force)
{
    my_M_PushMenu(name, frame, force);
}

//Called before SoF.exe entrypoint.
static void scaledMenu_EarlyStartup(void) {
  
    //Per Object Type fixes...
    //Fixes the position of the slider so it can rescale?(centers it?)
    //Careful _NOT_ to double-scale these because GetPicSize is often called.
    //Specifically in object_c::Setup() function.
    //Unlikely we need to use the draw hook..
    // orig_slider_c_Draw = DetourCreate(0x200D1110 , &my_slider_c_Draw, DETOUR_TYPE_JMP, 5);
    // orig_loadbox_c_Draw = DetourCreate(0x200DC250, &my_loadbox_c_Draw, DETOUR_TYPE_JMP, 7);
    // orig_vbar_c_Draw = DetourCreate(0x200D5800, &my_vbar_c_Draw, DETOUR_TYPE_JMP, 5);
    
    //=====================================================
    //===============STM ELEMENT PARSING===================
    //Stm resize keyword scale - now handled by REGISTER_HOOK_LEN macro
    
    // frame element width height scale
    // WriteE8Call(0x200E80CF, my_frame_c_Constructor); 
    
    //parsing width height on menu options
    WriteE8Call((void*)0x200CF8BA, (void*)&my_rect_c_Parse);

    //=====================================================
    //=====================================================

    //Menu Pictures sizing override except for icons/teamicons/vend
    //This that need to remain the same size ... for now...
    WriteE8Call((void*)0x200C8856, (void*)&my_Draw_GetPicSize);
    WriteByte((void*)0x200C885B, 0x90);
    
    //stm_c::Render(void) - can be useful in future because can get type here
    //Not used atm.
    //WriteE8Call(0x200EA48B, &my_master_Draw);

    //Filtering menus by 'filename' .rmf (To conditionally resize frame width height)
    // orig_M_PushMenu = DetourCreate( 0x200C7630 , &my_M_PushMenu, DETOUR_TYPE_JMP, 6);

    //Rescaling blank ( used for clickboxes/indentation/margins etc )
    //blank <width> <height>
    //It makes more sense to do this in ParseStm...
    // orig_stm_c_ParseBlank = DetourCreate ( 0x200E81E0  , &my_stm_c_ParseBlank, DETOUR_TYPE_JMP,  7);
    
}

static void scaledMenu_RefDllLoaded(void) {
    orig_Draw_GetPicSize = (void(*)(int*, int*, char*))*(int*)0x204035B4;

    /*
        Hooks are now automatically registered using REGISTER_HOOK_LEN macros.
        The hook manager intelligently checks addresses to determine which module they belong to.
        No manual DetourCreate calls needed.
    */
}

#endif // FEATURE_MENU_SCALING
