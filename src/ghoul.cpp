#include "ighoul.h"

IGhoul * ghoulmain 	= 	NULL;
unsigned int * clientinst 	= 	NULL;
unsigned int * objinst = NULL;

GetGhoul_type orig_GetGhoul = (GetGhoul_type)0x20069630;
FindClientInst_type orig_FindClientInst = nullptr; //0x2006CBC0;
GetXForm_type orig_GetXForm = nullptr;
SetTintOnAll_type orig_SetTintOnAll = nullptr; // 40
SetTint_type orig_SetTint = nullptr;
GetTint_type orig_GetTint = (GetTint_type)NULL;
AddNoteCallBack_type orig_AddNoteCallBack = (AddNoteCallBack_type)NULL;
GetUserData_type orig_GetUserData = (GetUserData_type)NULL; //58
GetNumChildren_type orig_GetNumChildren = (GetNumChildren_type)NULL; // 80
GetChild_type orig_GetChild = (GetChild_type)NULL; // 84
GetGhoulObject_type orig_GetGhoulObject = (GetGhoulObject_type)NULL;
FindPart_type orig_FindPart = (FindPart_type)NULL;
UnPackReliable_type orig_UnPackReliable=(UnPackReliable_type)NULL;
GetBoltMatrix_type orig_GetBoltMatrix=(GetBoltMatrix_type)NULL;
GetBoundBox_type orig_GetBoundBox=(GetBoundBox_type)NULL;
FindNoteToken_type orig_FindNoteToken = (FindNoteToken_type)NULL;

void PrintFunctionAddr(void)
{
	orig_UnPackReliable = (UnPackReliable_type)*(unsigned int*)((*(unsigned int*)(ghoulmain))+0x60);
	//printf("UnPackReliable is at %08X\n",ghoulmain);
}
/*
Class function of iGhoul
*/
bool GhoulInstFromID(short int ID)
{
	clientinst=NULL;
	orig_FindClientInst = (FindClientInst_type)*(unsigned int*)((*(unsigned int*)(ghoulmain))+0x24);
	asm volatile("movl %1,%%ecx;"//ghoulmain->ecx
					"push %2;"//push ID
					"call *%3;"//call pr_real_FindClientInst
					"movl %%eax,%0;"//eax->clientinst
					:"=m"(clientinst):"m"(ghoulmain),"m"(ID),"m"(orig_FindClientInst):"%ecx");
	if ( (int)clientinst > 0 )
		return true;
	return false;
}
/*
Class function of iGhoul
*/
bool GhoulGetInst(int slot)
{
	clientinst=NULL;
	slot+=1;
	unsigned int UUID=(slot) << 5;
	UUID=(UUID+slot)*12;
	short int * somearray=(short int*)(0x2015EF94+UUID);
	//orig_Com_Printf("somearray is %08X\nand contains %i\n",somearray,*somearray);
	if ( *somearray > 0 )
	{
		//orig_Com_Printf("UUID is int %hu\n",*somearray);
		orig_FindClientInst = (FindClientInst_type)*(unsigned int*)((*(unsigned int*)(ghoulmain))+0x24);
		asm volatile(
		"push %2;"//push somearray
		"movl %1,%%ecx;"//ghoulmain->ecx
		"call *%3;"//call orig_FindClientInst
		"movl %%eax,%0;" //eax->clientinst
		:"=m"(clientinst)
		:"m"(ghoulmain),"m"(*somearray),"m"(orig_FindClientInst)
		:"memory","%ecx"
		);
		if ( (int)clientinst > 0 )
			return true;
		return false;
	}
	return false;
}
/*
Class function of GhoulInst
*/
void GhoulSetTintOnAll(float r,float g,float b,float alpha)
{
	orig_SetTintOnAll = (SetTintOnAll_type)*(unsigned int*)((*(unsigned int*)(clientinst)) + 0x40);
	asm volatile(	"push %1;"//push alpha
					"push %2;"//push b
					"push %3;"//push g
					"push %4;"//push r
					"movl %0,%%ecx;"//clientinst->ecx
					"call *%5;"//call orig_SetTintOnAll
					::"m"(clientinst),"m"(alpha),"m"(b),"m"(g),"m"(r),"m"(orig_SetTintOnAll):"%ecx");
}

/*
Class function of GhoulInst
*/
void GhoulGetTint(float *r,float *g,float *b,float *a)
{
	orig_GetTint = (GetTint_type)*(unsigned int*)((*(unsigned int*)(clientinst)) + 0x3C);
	asm volatile(	"push %1;"//push alpha
					"push %2;"//push b
					"push %3;"//push g
					"push %4;"//push r
					"movl %0,%%ecx;"//clientinst->ecx
					"call *%5;"//call pr_real_GetTint
					::"m"(clientinst),"m"(a),"m"(b),"m"(g),"m"(r),"m"(orig_GetTint):"%ecx");
}

void GhoulSetTint(float r,float g,float b,float alpha)
{
	orig_SetTint = (SetTint_type)*(unsigned int*)((*(unsigned int*)(clientinst)) + 0x38);
	asm volatile(	"push %1;"//push alpha
					"push %2;"//push b
					"push %3;"//push g
					"push %4;"//push r
					"movl %0,%%ecx;"//clientinst->ecx
					"call *%5;"//call orig_SetTintOnAll
					::"m"(clientinst),"m"(alpha),"m"(b),"m"(g),"m"(r),"m"(orig_SetTint):"%ecx");
}

void GhoulAddNoteCallBack(IGhoulCallBack *c,GhoulID Token)
{
	orig_AddNoteCallBack = (AddNoteCallBack_type)*(unsigned int*)((*(unsigned int*)(clientinst)) + 0x5C);
	asm volatile(	"push %1;"//push Token
					"push %2;"//push c
					"movl %0,%%ecx;"//clientinst->ecx
					"call *%3;"//call orig_SetTintOnAll
					::"m"(clientinst),"m"(Token),"m"(c),"m"(orig_AddNoteCallBack):"%ecx");
}

/*
Class function of GhoulInst
*/
void GhoulGetXform(float *m)
{
	orig_GetXForm=(GetXForm_type)*(unsigned int*)((*(unsigned int*)(clientinst)) + 0x04);
	asm volatile(	"push %1;"//push m
					"movl %0,%%ecx;"//clientinst->ecx
					"call *%2;"//call orig_GetXForm
					::"m"(clientinst),"m"(m),"m"(orig_GetXForm):"%ecx");
}

/*
Class function of GhoulInst
*/
bool GhoulGetObject(void)
{
	objinst=NULL;
	orig_GetGhoulObject=(GetGhoulObject_type)*(unsigned int*)((*(unsigned int*)(clientinst)) + 0x88);
	asm volatile("movl %1,%%ecx;"//clientinst->ecx
					"call *%2;"//call orig_GetGhoulObject
					"movl %%eax,%0;"//eax->objinst
					:"=m"(objinst):"m"(clientinst),"m"(orig_GetGhoulObject):"%ecx");
	if ((int)objinst > 0 )
		return true;
	return false;
}

/*
Class function of ObjInst
*/
unsigned short GhoulFindPart(const char * partname)
{
	unsigned int partid;
	orig_FindPart=(FindPart_type)*(unsigned int*)((*(unsigned int*)(objinst)) + 0x1C);

	asm volatile(	"push %2;"//push partname
					"movl %1,%%ecx;"//objinst->ecx
					"call *%3;"//call orig_FindPart
					"movl %%eax,%0;"//eax->partid
					:"=m"(partid):"m"(objinst),"m"(partname),"m"(orig_FindPart):"%ecx");
	
	//asm volatile("movl %0,%%ecx;"::"m"(objinst):"%ecx");
	//return pr_real_FindPart(partname);
	return (unsigned short)partid;
}

/*
Class function of ObjInst
*/
GhoulID GhoulFindNoteToken(const char *Token)
{
	unsigned int ret;
	orig_FindNoteToken=(FindNoteToken_type)*(unsigned int*)((*(unsigned int*)(objinst)) + 0x30);
	asm volatile(	"push %2;"//push Token
					"movl %1,%%ecx;"//objinst->ecx
					"call *%3;"//call orig_FindNoteToken
					"movl %%eax,%0;"//eax->ret
					:"=m"(ret):"m"(objinst),"m"(Token),"m"(orig_FindNoteToken):"%ecx");
	return (unsigned short)ret;
}

/*
Class function of GhoulInst
*/
bool GhoulBoltMatrix(float Time,Matrix4 &m,unsigned short GhoulID,MatrixType kind,bool ToRoot)
{
	int ret = 0;
	orig_GetBoltMatrix = (GetBoltMatrix_type)*(unsigned int*)((*(unsigned int*)(clientinst))+0xC8);
	asm volatile(	"push %2;" //push ToRoot
					"push %3;" //push kind
					"push %4;" //push GhoulID
					"push %5;" //push m
					"push %6;" //push Time
					"movl %1,%%ecx;" //clientinst -> ecx
					"call *%7;" //call orig_GetBoltMatrix
					"movl %%eax,%0;" //eax->ret
					:"=m"(ret)
					:"m"(clientinst),"m"(ToRoot),"m"(kind),"m"(GhoulID),"m"(&m),"m"(Time),"m"(orig_GetBoltMatrix):"%ecx");
	if ( (int)ret > 0 )
		return true;
	return false;

}

void GhoulBoundBox(float Time,const Matrix4 &ToWorld,Vect3 &mins,Vect3 &maxs,GhoulID Part,bool Accurate)
{
	orig_GetBoundBox = (GetBoundBox_type)*(unsigned int*)((*(unsigned int*)(clientinst))+0xCC);
	asm volatile("push %1;" // Accurate
					"push %2;" // Part
					"push %3;" // maxs
					"push %4;" // mins
					"push %5;" // ToWorld
					"push %6;" // Time
					"movl %0,%%ecx;" // clientinst->ecx
					"call *%7;" // orig_GetBoundBox
					::"m"(clientinst),"m"(Accurate),"m"(Part),"m"(&maxs),"m"(&mins),"m"(&ToWorld),"m"(Time),"m"(orig_GetBoundBox):"%ecx");
}
