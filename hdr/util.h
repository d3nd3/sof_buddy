#define PRINT_GOOD 1
#define PRINT_BAD 2
#define PRINT_LOG 3
#define PRINT_LOG_EMPTY 4

// Log Debug Logs ? Else only Good/Bad (Status).
// #define __LOGGING__

#define M_PI   3.14159265358979323846264338327950288
#define degToRad(angleInDegrees) ((angleInDegrees) * M_PI / 180.0)
#define radToDeg(angleInRadians) ((angleInRadians) * 180.0 / M_PI)

#define P_WHITE		"\001"
#define P_RED  		"\002"
#define P_GREEN		"\003"
#define P_YELLOW	"\004"
#define P_BLUE		"\005"
#define P_PURPLE	"\006"
#define P_CYAN		"\007"
#define P_BLACK		"\010"
#define P_HWHITE  	"\011"
#define P_DONT_USE1	"\012"
#define P_HRED    	"\013"
#define P_HGREEN  	"\014"
#define P_DONT_USE2	"\015"
#define P_HYELLOW	"\016"
#define P_HBLUE		"\017"
#define P_CAMOBROWN	"\020"
#define P_CAMOGREEN	"\021"
#define P_SEAGREEN 	"\022"
#define P_SEABLUE  	"\023"
#define P_METAL    	"\024"
#define P_DBLUE    	"\025"
#define P_DPURPLE  	"\026"
#define P_DGREY    	"\027"
#define P_PINK	  	"\030"
#define P_BLOODRED 	"\031"
#define P_RUSSET   	"\032"
#define P_BROWN    	"\033"
#define P_TEXT     	"\034"
#define P_BAIGE    	"\035"
#define P_LBROWN   	"\036"
#define P_ORANGE   	"\037"

void PrintOut(int mode,char *msg,...);

void writeUnsignedIntegerAt(void * addr, unsigned int value);
void writeIntegerAt(void * addr, int value);
void WriteE8Call(void * where,void * dest);
void WriteE9Jmp(void * where, void * dest);
void WriteByte(void * where, unsigned char value);