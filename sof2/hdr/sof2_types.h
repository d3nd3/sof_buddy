#pragma once

typedef int qboolean;

typedef struct cvar_s {
	char* name;
	char* string;
	char* latched_string;
	int flags;
	void (*command)(struct cvar_s*);
	qboolean modified;
	float value;
	struct cvar_s* next;
} cvar_t;

typedef void (*xcommand_t)(void);
typedef void (*cvarcommand_t)(cvar_t*);

#define CVAR_ARCHIVE 0x00000001
#define CVAR_USERINFO 0x00000002
#define CVAR_SERVERINFO 0x00000004
#define CVAR_NOSET 0x00000008
#define CVAR_LATCH 0x00000010
#define CVAR_INTERNAL 0x00000080
