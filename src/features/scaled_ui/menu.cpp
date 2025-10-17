/*
	Scaled UI - Menu Scaling Functions (Experimental)
	
	This file contains all menu scaling functionality including:
	- Menu element scaling
	- Menu text scaling
	- Menu rendering hooks
	- Menu parsing and layout modifications
*/

#include "feature_config.h"

#if FEATURE_UI_SCALING

#ifdef UI_MENU

#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "shared_hook_manager.h"
#include "feature_macro.h"
#include "scaled_ui.h"

#include "DetourXS/detourxs.h"
#include <math.h>
#include <stdint.h>
#include <string>

// Menu scaling function implementations
const char* get_nth_entry(const char* str, int n) {
    if (!str) return nullptr;
    
    const char* current = str;
    int count = 0;
    
    while (*current && count < n) {
        if (*current == '/') {
            count++;
        }
        current++;
    }
    
    if (count == n && *current) {
        return current;
    }
    
    return nullptr;
}

/*
    This function is called internally by:
    - cInterface::DrawNum()
    - cDMRanking::Draw()
    - SCR_FadePic()
    - SCR_TouchPics()
    - SCR_Crosshair()
    - vbar_c::vbar_c()
    - slider_c::Setup()
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
                extern void (*orig_Draw_GetPicSize)(int*, int*, char*);
                orig_Draw_GetPicSize (w, h, pic);
                return;
            } else if (len == 8 && strncmp(thirdEntry, "backdrop", 8) == 0) {
                /*
                  This idea is good!, except the tiling system is forcing chunks of 128 it seems?
                  So if doesn't fully divide by 128, it gives nearest, leaving empty space.
                */
                extern void (*orig_Draw_GetPicSize)(int*, int*, char*);
                orig_Draw_GetPicSize (w , h, pic);
                *w = *w * screen_y_scale;
                *h = *h * screen_y_scale;
                return;
            } else if (len == 4 && strncmp(thirdEntry, "misc", 4) == 0) {
                // Check if the last 4 characters of the path are "vend"
                size_t picLen = strlen(pic);
                if (picLen >= 4 && strncmp(pic + picLen - 4, "vend", 4) == 0) {
                    extern void (*orig_Draw_GetPicSize)(int*, int*, char*);
                    orig_Draw_GetPicSize(w, h, pic);
                    // *w = *w * screen_y_scale;
                    // *h = *h * screen_y_scale;
                    return;
                }
            }
        }
    }
    // orig_Com_Printf("pic %s\n",pic);
    extern void (*orig_Draw_GetPicSize)(int*, int*, char*);
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
/*
    For : stm_c_ParseStm, edits the string/stream to resize the rects.
*/
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
    
    if (!changed) {
        return;
    }
    
    extern void * (__cdecl *orig_new_std_string)(int length);
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

/*
    Called by my_rect_c_Parse to resize the width and height of the rects.
*/
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
    
    
    extern void * (__cdecl *orig_new_std_string)(int length);
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
    Called by stm_c_ParseStm
    This represents the invisibile space, like tabs and indentation
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
    extern void * (__cdecl *orig_new_std_string)(int length);
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
Repositions the slider with Draw_StretchPic
We were double scaling this, because Draw_GetPicSize is being used for this. duh.
*/
void __thiscall my_slider_c_Draw(void * self) {
    extern bool menuSliderDraw;
    menuSliderDraw = true;
    extern void (__thiscall *orig_slider_c_Draw)(void * self);
    orig_slider_c_Draw(self);
    menuSliderDraw = false;
}

/*
Was going to try and scale the loadGame screenshots,
but seems too hard atm. Disabled
*/
void __thiscall my_loadbox_c_Draw(void * self) {
    extern bool menuLoadboxDraw;
    menuLoadboxDraw = true;
    
    extern void (__thiscall *orig_loadbox_c_Draw)(void * self);
    orig_loadbox_c_Draw(self);
    menuLoadboxDraw = false;
}
/*
Disabled currently, but enables custom control of this object.
*/
void __thiscall my_vbar_c_Draw(void * self) {
    extern bool menuVerticalScrollDraw;
    menuVerticalScrollDraw = true;
   
    // extern void (__thiscall *orig_vbar_c_Draw)(void * self);
    // orig_vbar_c_Draw(self);
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
        
        
        extern void (__thiscall *orig_frame_c_Constructor)(void*, void*, char*, char*, void*);
        orig_frame_c_Constructor(self,menu_c,final_width,final_height,frame_name);
    } else { 
        extern void (__thiscall *orig_frame_c_Constructor)(void*, void*, char*, char*, void*);
        orig_frame_c_Constructor(self,menu_c,width,height,frame_name);
    }
}

/*
// Hypothetical structure for toke_c
class toke_c {
public:
// [this+0]
char* m_pBuffer;        // Pointer to the beginning of the allocated memory.

// [this+4]
char* m_pCursor;        // Pointer to the current read/write position (cursor) within the buffer.

// [this+8]
char* m_pEndOfBuffer;   // Pointer to the end of the allocated memory (one past the last valid byte).

// [this+12]
// This is actually how many lines have been passed

// [this+16] ([esi+10h])
size_t m_capacity;      // The total size/capacity of the allocated buffer.
};

*/
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
    extern char * (__thiscall *ostm_c_ParseStm)(void*, void*);
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
// Text measurement hooks moved to text.cpp

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
        extern void (__cdecl *oM_PushMenu)(const char*, const char*, bool);
        oM_PushMenu(name,frame,force);
        isMenuSpmSettings = false;
        return;
    }
    extern void (__cdecl *oM_PushMenu)(const char*, const char*, bool);
    oM_PushMenu(name,frame,force);
}

// Hook function for M_PushMenu
void hkM_PushMenu(const char * name, const char * frame, bool force)
{
    my_M_PushMenu(name, frame, force);
}

#endif // UI_MENU

#endif // FEATURE_UI_SCALING
