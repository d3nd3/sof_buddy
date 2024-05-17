extern void mediaTimers_apply(void);
extern void mediaTimers_early(void);

extern void refFixes_apply(void);

extern void scaledFont_apply(void);

extern void resetTimers(int val);
extern int oldtime;

#define FEATURE_MEDIA_TIMERS
#define FEATURE_HD_TEX
#define FEATURE_TEAMICON_FIX
// #define FEATURE_ALT_LIGHTING
#define FEATURE_FONT_SCALING
