/*
 * gcp_dev.c (c) Chris Young 2017
 * Somewhat based on lpr.device by Olaf Barthel
 * and my previous device attempts
 */

#include <intuition/intuition.h>

#include <exec/memory.h>
#include <exec/devices.h>
#include <exec/resident.h>
#include <exec/errors.h>
#include <exec/io.h>

#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <dos/rdargs.h>
#include <dos/stdio.h>

#include <devices/parallel.h>
#include <devices/newstyle.h>

#ifndef __amigaos4__
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#endif
#include <clib/alib_protos.h>

#ifdef __SASC
#include <pragmas/exec_sysbase_pragmas.h>
#include <pragmas/dos_pragmas.h>
#include <pragmas/intuition_pragmas.h>
#include <pragmas/locale_pragmas.h>
#else
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#endif

#define USE_BUILTIN_MATH
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __amigaos4__
struct Library *NewlibBase;
struct Interface *INewlib;

//struct DOSIFace *IDOS;
//struct ExecIFace *IExec;
#endif

#ifndef FFlush
#define FFlush Flush
#endif

#define memcpy(a,b,c)	CopyMem(b,a,c)

/****************************************************************************/

#include "token.h"
#include "gcp_rev.h"
#include "devbase.h"
#include "gcp.h"

/****************************************************************************/

typedef unsigned char *	KEY;
typedef long			SWITCH;
typedef long *			NUMBER;

/****************************************************************************/

#define FLAG_IS_SET(v,f)	(((v) & (f)) != 0)
#define FLAG_IS_CLEAR(v,f)	(((v) & (f)) == 0)

#define SET_FLAG(v,f)		v |=  (f)
#define CLEAR_FLAG(v,f)		v &= ~(f)

#define OK					(0)

#define NOT					!

#define NUM_ENTRIES(t)		(sizeof(t) / sizeof(t[0]))

#define MAX_FILENAME_LEN	256

/****************************************************************************/

#ifndef ZERO
#define ZERO ((BPTR)NULL)
#endif

/****************************************************************************/

/*
APTR ASM AsmAllocPooled(REG(a0) APTR poolHeader,REG(d0) ULONG memSize,REG(a6) struct Library *);
APTR ASM AsmCreatePool(REG(d0) ULONG memFlags,REG(d1) ULONG puddleSize,REG(d2) ULONG threshSize,REG(a6) struct Library *);
VOID ASM AsmDeletePool(REG(a0) APTR poolHeader,REG(a6) struct Library *);
*/
#define AsmAllocPooled(a,b,c)		AllocPooled(a,b)
#define AsmCreatePool(a,b,c,d)		CreatePool(a,b,c)
#define AsmDeletePool(a,b)			DeletePool(a)

/****************************************************************************/


#define SIGF_Wakeup	SIGF_SINGLE

/****************************************************************************/

#define CMD_OpenUnit	(CMD_NONSTD+400)
#define CMD_CloseUnit	(CMD_NONSTD+401)

/****************************************************************************/

LONG ReturnError(VOID) { return(-1); }

/****************************************************************************/

STATIC VOID
SendCmd(
	struct DeviceBase *	db,
	struct DeviceUnit *	du,
	UWORD				command)
{
	struct IORequest ior;
	struct MsgPort port;

	memset(&port,0,sizeof(port));
	port.mp_Flags	= PA_SIGNAL;
	port.mp_SigBit	= SIGB_SINGLE;
	port.mp_SigTask	= FindTask(NULL);
	NewList(&port.mp_MsgList);

	memset(&ior,0,sizeof(ior));
	ior.io_Message.mn_ReplyPort	= &port;
	ior.io_Message.mn_Length	= sizeof(ior);
	ior.io_Unit					= (struct Unit *)du;
	ior.io_Device				= (struct Device *)db;
	ior.io_Command				= command;

	SetSignal(0,SIGF_SINGLE);

	PutMsg(&ior.io_Unit->unit_MsgPort,(struct Message *)&ior);
	WaitPort(&port);
}

/****************************************************************************/

#ifdef __amigaos4__
STATIC VOID
DevBeginIO(
	struct Interface *  Self,
	struct IOStdReq *	ior)
{
	struct DeviceBase *db = (struct DeviceBase *) Self->Data.LibBase;
#else
STATIC VOID ASM
DevBeginIO(
	REG(a1, struct IOStdReq *	ior),
	REG(a6, struct DeviceBase *	db))
{
#endif
	struct DeviceUnit * du = (struct DeviceUnit *)ior->io_Unit;

	ior->io_Message.mn_Node.ln_Type	= NT_MESSAGE;
	ior->io_Error					= OK;

	switch(ior->io_Command)
	{
		case CMD_READ:

			ior->io_Actual = 0;
			break;

		case CMD_START:
		case CMD_STOP:
		case CMD_FLUSH:

			break;

		case CMD_WRITE:
		case CMD_CLEAR:
		case CMD_RESET:
		case GCPCMD_LIST:

			CLEAR_FLAG(ior->io_Flags,IOF_QUICK);
			SET_FLAG(ior->io_Flags,IOPARF_QUEUED);
			PutMsg(&du->du_Unit.unit_MsgPort,(struct Message *)ior);

			ior = NULL;
			break;

		case PDCMD_QUERY:

			((struct IOExtPar *)ior)->io_Status = 0;
			break;

		case PDCMD_SETPARAMS:

			if(FLAG_IS_SET(((struct IOExtPar *)ior)->io_ParFlags,PARF_SHARED))
				ior->io_Error = ParErr_DevBusy;

			break;

		case NSCMD_DEVICEQUERY:

			if(ior->io_Data != NULL && ior->io_Length >= sizeof(struct NSDeviceQueryResult))
			{
				STATIC UWORD SupportedCommands[] =
				{
					CMD_READ,
					CMD_START,
					CMD_STOP,
					CMD_RESET,
					CMD_CLEAR,
					CMD_WRITE,
					CMD_FLUSH,
					
					PDCMD_QUERY,
					PDCMD_SETPARAMS,

					NSCMD_DEVICEQUERY,

					GCPCMD_LIST,

					0
				};

				struct NSDeviceQueryResult * qr = ior->io_Data;

				qr->SizeAvailable		= 16;
				qr->DeviceType			= NSDEVTYPE_PARALLEL;
				qr->DeviceSubType		= 0;
				qr->SupportedCommands	= SupportedCommands;

				ior->io_Actual = 16;
			}
			else
			{
				ior->io_Error = IOERR_BADLENGTH;
			}

			break;

		default:

			ior->io_Error = IOERR_NOCMD;
			break;
	}

	if(ior != NULL && FLAG_IS_CLEAR(ior->io_Flags,IOF_QUICK))
		ReplyMsg((struct Message *)ior);
}

/****************************************************************************/

STATIC VOID UnitProcessEntry(VOID);

/****************************************************************************/

#ifdef __amigaos4__
STATIC LONG
DevAbortIO(
	struct Interface *	Self,
	struct IORequest *	ior)
{
	struct DeviceBase *db = (struct DeviceBase *)Self->Data.LibBase;
#else
STATIC LONG ASM
DevAbortIO(
	REG(a6, struct DeviceBase *	db),
	REG(a1, struct IORequest *	ior))
{
#endif
	LONG result = OK;

	Forbid();

	if(ior->io_Message.mn_Node.ln_Type != NT_REPLYMSG &&
	   FLAG_IS_CLEAR(ior->io_Flags,IOF_QUICK))
	{
		if(FLAG_IS_SET(ior->io_Flags,IOPARF_QUEUED))
		{
			Remove((struct Node *)ior);

			CLEAR_FLAG(ior->io_Flags,IOPARF_QUEUED);
			SET_FLAG(ior->io_Flags,IOPARF_ABORT);

			ior->io_Error = result = IOERR_ABORTED;

			ReplyMsg((struct Message *)ior);
		}
	}

	Permit();

	return(result);
}

/****************************************************************************/

#ifdef __amigaos4__
STATIC struct DeviceBase *
DevInit(
	struct DeviceBase *	db,
	BPTR				segList,
	struct ExecIFace *	exec)
{
	struct Library *ExecBase = exec->Data.LibBase;

	IExec = exec;
#else
STATIC struct DeviceBase * ASM
DevInit(
	REG(a0, BPTR				segList),
	REG(d0, struct DeviceBase *	db),
	REG(a6, struct Library *	ExecBase))
{
#endif
	struct DeviceBase * result = NULL;

	db->db_SysBase = ExecBase;

	db->access_token = NULL;
	db->refresh_token = NULL;
	db->access_token_expiry = 0;

	if(SysBase->lib_Version >= 37)
	{
		db->db_SegList = segList;
		db->db_Library.lib_Revision = REVISION;

		db->db_Counter = 0;

		InitSemaphore(&db->db_Lock);

		DOSBase = OpenLibrary("dos.library",37);
		IntuitionBase = OpenLibrary("intuition.library",37);
		UtilityBase = OpenLibrary("utility.library",39);

		#ifdef __amigaos4__
		NewlibBase = OpenLibrary("newlib.library",50);
		INewlib = GetInterface(NewlibBase,"main",1L,NULL);
		if (NULL == INewlib)
		{
			CloseLibrary(NewlibBase);
			NewlibBase = NULL;
		}

		IDOS = (struct DOSIFace *) GetInterface(DOSBase,"main",1L,NULL);
		if (NULL == IDOS)
		{
			CloseLibrary(DOSBase);
			DOSBase = NULL;
		}

		IIntuition = (struct IntuitionIFace *) GetInterface(IntuitionBase,"main",1L,NULL);
		if (NULL == IIntuition)
		{
			CloseLibrary(IntuitionBase);
			IntuitionBase = NULL;
		}

		IUtility = (struct UtilityIFace *) GetInterface(UtilityBase,"main",1L,NULL);
		if (NULL == IUtility)
		{
			CloseLibrary(UtilityBase);
			UtilityBase = NULL;
		}

		#endif

		if(DOSBase != NULL && IntuitionBase != NULL && NewlibBase != NULL && UtilityBase != NULL)
		{
			ULONG i;

			memset(db->db_Unit,0,sizeof(db->db_Unit));

			for(i = 0 ; i < NUM_ENTRIES(db->db_Unit) ; i++)
			{
				db->db_Unit[i].du_Process	= NULL;
				db->db_Unit[i].du_Index		= i;

				db->db_Unit[i].du_Unit.unit_MsgPort.mp_Flags	= PA_SIGNAL;
				db->db_Unit[i].du_Unit.unit_MsgPort.mp_SigBit	= SIGBREAKB_CTRL_F;
				NewList(&db->db_Unit[i].du_Unit.unit_MsgPort.mp_MsgList);

				db->db_Unit[i].du_Unit.unit_flags	= 0;
				db->db_Unit[i].du_Unit.unit_pad		= 0;
				db->db_Unit[i].du_Unit.unit_OpenCnt	= 0;
			}

			result = db;
		}
		else
		{
			#ifdef __amigaos4__
			DropInterface((struct Interface *)IUtility);
			DropInterface((struct Interface *)IDOS);
			DropInterface((struct Interface *)IIntuition);
			DropInterface((struct Interface *)INewlib);
			CloseLibrary(NewlibBase);
			#endif

			CloseLibrary(UtilityBase);
			CloseLibrary(DOSBase);
			CloseLibrary(IntuitionBase);
		}
	}

	if(result == NULL)
	{
		#ifdef __amigaos4__
		DeleteLibrary((struct Library *)db);
		#else
		FreeMem((BYTE *)db - db->db_Library.lib_NegSize,
		        db->db_Library.lib_NegSize + db->db_Library.lib_PosSize);
		#endif
	}

	return(result);
}

/****************************************************************************/

#ifdef __amigaos4__
STATIC LONG
DevOpen(
	struct Interface *  Self,
	struct IORequest *	ior,
	ULONG				unitNumber,
	ULONG				flags)
{
	struct DeviceBase *db = (struct DeviceBase *)Self->Data.LibBase;
#else
STATIC LONG ASM
DevOpen(
	REG(d0, ULONG				unitNumber),
	REG(d1, ULONG				flags),
	REG(a1, struct IORequest *	ior),
	REG(a6, struct DeviceBase *	db))
{
#endif
	LONG result = ParErr_InitErr;

	if(0 <= unitNumber && unitNumber < NUM_ENTRIES(db->db_Unit))
	{
		struct DeviceUnit * du = &db->db_Unit[unitNumber];
		BOOL unlocked = FALSE;

		db->db_Library.lib_OpenCnt++;

		ObtainSemaphore(&db->db_Lock);

		if(db->db_Library.lib_OpenCnt == 1)
		{
#if 0
			if(db->db_LocaleBase == NULL)
			{
				db->db_LocaleBase = OpenLibrary("locale.library",38);

				#ifdef __amigaos4__
				ILocale = (struct LocaleIFace *) GetInterface(LocaleBase,"main",1L,NULL);
				if (ILocale == NULL)
				{
					CloseLibrary(LocaleBase);
					LocaleBase = NULL;
				}
				#endif

				if(db->db_LocaleBase != NULL)
					db->db_Catalog = OpenCatalog(NULL,"lpr.catalog",0UL);
			}
#endif
		}

		if(du->du_Unit.unit_OpenCnt == 0)
		{
			du->du_Unit.unit_OpenCnt++;

			ReleaseSemaphore(&db->db_Lock);
			unlocked = TRUE;

			du->du_Process = CreateNewProcTags(
				NP_Entry,		UnitProcessEntry,
				NP_Name,		db->db_Library.lib_Node.ln_Name,
				NP_ConsoleTask,	NULL,
				NP_StackSize,	131072,
			TAG_DONE);
			if(du->du_Process != NULL)
			{
				du->du_Unit.unit_MsgPort.mp_SigTask = du->du_Process;

				du->du_Process->pr_Task.tc_UserData = du;

				Signal((struct Task *)du->du_Process,SIGF_Wakeup);

				SendCmd(db,du,CMD_OpenUnit);

				ior->io_Unit = (struct Unit *)du;

				ior->io_Message.mn_Node.ln_Type = NT_REPLYMSG;
				ior->io_Unit->unit_OpenCnt++;

				db->db_Library.lib_OpenCnt++;

				result = OK;
			}

			du->du_Unit.unit_OpenCnt--;
		}

		if(NOT unlocked)
			ReleaseSemaphore(&db->db_Lock);

		db->db_Library.lib_OpenCnt--;
	}
	else
	{
		result = IOERR_OPENFAIL;
	}

	if(result != OK)
	{
		ior->io_Device	= NULL;
		ior->io_Unit	= NULL;
	}

	ior->io_Error = result;

	return(result);
}

/****************************************************************************/

#ifdef __amigaos4__
STATIC BPTR
DevExpunge(struct Interface * Self)
{
	struct DeviceBase *db = (struct DeviceBase *)Self->Data.LibBase;
#else
STATIC BPTR ASM
DevExpunge(REG(a6, struct DeviceBase * db))
{
#endif
	BPTR result = ZERO;

	if(db->db_Library.lib_OpenCnt == 0)
	{
	
//		cleanup(db); // maybe on close() is better?
	
		Remove((struct Node *)db);

		#ifdef __amigaos4__
		DropInterface((struct Interface *)IUtility);
		DropInterface((struct Interface *)IDOS);
		DropInterface((struct Interface *)IIntuition);
		DropInterface((struct Interface *)INewlib);
		CloseLibrary(NewlibBase);
		#endif

		CloseLibrary(UtilityBase);
		CloseLibrary(DOSBase);
		CloseLibrary(IntuitionBase);

		result = db->db_SegList;

		#ifdef __amigaos4__
		DeleteLibrary((struct Library *)db);
		#else
		FreeMem((BYTE *)db - db->db_Library.lib_NegSize,
		        db->db_Library.lib_NegSize + db->db_Library.lib_PosSize);
		#endif
	}
	else
	{
		SET_FLAG(db->db_Library.lib_Flags,LIBF_DELEXP);
	}

	return(result);
}

/****************************************************************************/

#ifdef __amigaos4__
STATIC BPTR
DevClose(
	struct Interface *  Self,
	struct IORequest *	ior)
{
	struct DeviceBase *db = (struct DeviceBase *)Self->Data.LibBase;
#else
STATIC BPTR ASM
DevClose(
	REG(a1, struct IORequest *	ior),
	REG(a6, struct DeviceBase *	db))
{
#endif
	BPTR result = ZERO;

	ObtainSemaphore(&db->db_Lock);

	if(ior->io_Unit != NULL)
	{
		struct DeviceUnit * du = (struct DeviceUnit *)ior->io_Unit;

		ior->io_Unit->unit_OpenCnt--;
		if(ior->io_Unit->unit_OpenCnt == 0)
			SendCmd(db,du,CMD_CloseUnit);
	}

	db->db_Library.lib_OpenCnt--;
	if(db->db_Library.lib_OpenCnt == 0)
	{
/*
		#ifdef __amigaos4__
		DropInterface((struct Interface *)ILocale);
		ILocale = NULL;
		#endif

		CloseLibrary(db->db_LocaleBase);
		db->db_LocaleBase = NULL;
*/
	}

	ReleaseSemaphore(&db->db_Lock);

	ior->io_Device	= NULL;
	ior->io_Unit	= NULL;

	if(db->db_Library.lib_OpenCnt == 0 && FLAG_IS_SET(db->db_Library.lib_Flags,LIBF_DELEXP))
		#ifdef __amigaos4__
		result = DevExpunge(Self);
		#else
		result = DevExpunge(db);
		#endif

	return(result);
}

/****************************************************************************/

#ifdef __amigaos4__

int _start(void)
{ return 30; }

STATIC ULONG Obtain(struct Interface *Self)
{
	return Self->Data.RefCount++;
}

/*****************************************************************************/

STATIC ULONG Release(struct Interface *Self)
{
	return Self->Data.RefCount--;
}

/*****************************************************************************/

STATIC CONST APTR manager_vectors[] =
{
	Obtain,
	Release,
	NULL,
	NULL,
	DevOpen,
	DevClose,
	DevExpunge,
	NULL,
	DevBeginIO,
	DevAbortIO,
	(APTR)-1
};

STATIC CONST struct TagItem devmanagerTags[] =
{
	{MIT_Name,             (ULONG)"__device"},
	{MIT_VectorTable,      (ULONG)manager_vectors},
	{MIT_Version,          1},
	{TAG_DONE,             0}
};

STATIC CONST APTR main_vectors[]=
{
  Obtain,
  Release,
  NULL,
  NULL,
  (APTR)-1
};

STATIC CONST struct TagItem mainTags[] =
{
        {MIT_Name,     			(ULONG)"main"},
        {MIT_VectorTable,		(ULONG)main_vectors},
        {MIT_Version,       1},
        {TAG_DONE,          0}
};

// MLT_INTERFACES array
STATIC CONST CONST_APTR libInterfaces[] =
{
	devmanagerTags,
	mainTags,
	NULL
};

extern CONST CONST_APTR VecTable68K[];

STATIC CONST struct TagItem initTab[] =
{
	{CLT_DataSize,         (ULONG)(sizeof(struct DeviceBase))},
	{CLT_Interfaces,       (ULONG)libInterfaces},
	{CLT_InitFunc,         (ULONG)DevInit},
	{CLT_Vector68K,		   (ULONG)VecTable68K},
	{TAG_DONE,             0}
};


STATIC const struct Resident DevTag =
{
	RTC_MATCHWORD,
	&DevTag,
	&DevTag + 1,
	RTF_AUTOINIT | RTF_NATIVE,
	VERSION,
	NT_DEVICE,
	0,
	"gcp.device",
	VSTRING,
	initTab
};

#else

STATIC ULONG
DevReserved(VOID)
{
	return(0);
}

/****************************************************************************/

struct DevInitData
{
	ULONG	did_LibraryBaseSize;
	APTR	did_FunctionVector;
	APTR	did_InitTable;
	APTR	did_InitRoutine;
};

/****************************************************************************/

STATIC const APTR FunctionVector[] =
{
	DevOpen,
	DevClose,
	DevExpunge,
	DevReserved,

	/************************************************************************/

	DevBeginIO,
	DevAbortIO,

	/************************************************************************/

	(APTR)-1
};

STATIC const struct DevInitData DevInitData =
{
	sizeof(struct DeviceBase),
	FunctionVector,
	NULL,
	DevInit
};

STATIC const struct Resident DevTag =
{
	RTC_MATCHWORD,
	&DevTag,
	&DevTag + 1,
	RTF_AUTOINIT,
	VERSION,
	NT_DEVICE,
	0,
	"gcp.device",
	VSTRING,
	&DevInitData
};
#endif

struct Resident * SAS_Hack = &DevTag;

/****************************************************************************/

#undef SysBase
#undef IExec

/****************************************************************************/

VOID VARARGS68K SPrintf(struct DeviceBase *,STRPTR,STRPTR,...);

/****************************************************************************/

STATIC VOID
FillFaultString(struct DeviceBase * db,LONG error,STRPTR buffer,LONG buffer_size)
{
	if(Fault(error,NULL,buffer,buffer_size) > 0)
	{
		LONG len = strlen(buffer);

		while(len > 0 && buffer[len-1] == '\n')
			buffer[--len] = '\0';
	}
	else
	{
		strcpy(buffer,"");
	}
}

/****************************************************************************/

struct MemNode
{
	struct MinNode	mn_MinNode;
	UBYTE			mn_Data[1000];
	UWORD			mn_Size;
};

/****************************************************************************/


STATIC VOID
UnitProcessEntry(VOID)
{
	struct Library * SysBase = (*(struct Library **)4L);
	#ifdef __amigaos4__
	struct ExecIFace * IExec = (struct ExecIFace *) ((struct ExecBase *)SysBase)->MainInterface;
	#endif
	struct Process * thisProcess;
	struct DeviceBase * db = NULL;
	struct DeviceUnit * du;
	struct MsgPort * port;
	struct IOStdReq * ior;
	BOOL done = FALSE;
	BOOL opened = FALSE;
	int error = 0;
	int ret;
	int bytes = 0;
	int count = 0;
	BPTR fh = ZERO;
	LONG file_size = 0;
	BOOL ready = FALSE;
	LONG mode = -1;
	char path[100];

	Wait(SIGF_Wakeup);

	thisProcess = (struct Process *)FindTask(NULL);
	du = thisProcess->pr_Task.tc_UserData;
	port = &du->du_Unit.unit_MsgPort;

	do
	{
		WaitPort(port);

		Forbid();

		while((ior = (struct IOStdReq *)GetMsg(port)) != NULL)
		{
			CLEAR_FLAG(ior->io_Flags,IOPARF_QUEUED);

			Permit();

			switch(ior->io_Command)
			{
				case CMD_OpenUnit:
					{
						db = (struct DeviceBase *)ior->io_Device;

//						if(path[0] == '\0') {
							SPrintf(db, path, "T:gcpspool%ld%ld.ps", du->du_Index, GetUniqueID());
							path[sizeof(path)-1] = '\0';
//						}

//						SPrintf(db, path, "%s%ld", path, GetUniqueID());

/*
						DebugPrintF("[gcp.device] Printer     %s\n", printer);
						DebugPrintF("[gcp.device] Spool file  %s\n", path);
						DebugPrintF("[gcp.device] Template    %s\n", template);
*/
						if(!opened) {
							fh = FOpen(path, MODE_NEWFILE, 0);
							opened = TRUE;
							file_size = 0;
						}
					}
					break;

				case CMD_CloseUnit:
					if(opened) FClose(fh);

					if(file_size > 0) {
						print_file(db, du->du_Index, path);
					}

					Delete(path);
					path[0] = '\0';
					done = TRUE;
					file_size = 0;

					opened = ready = FALSE;

					break;

				case CMD_CLEAR:

					break;

				case CMD_RESET:


					break;

				case CMD_WRITE:

					ior->io_Actual = 0;

					if(opened)
					{
						LONG len;

						len = ior->io_Length;
						if(len == -1)
							len = strlen(ior->io_Data);

						if(len > 0)
						{
							if(bytes = FWrite(fh, ior->io_Data, 1, len))
							{
								ior->io_Actual = bytes;
								file_size += bytes;
							}
							else
							{
							/*
								ShowEasyRequest(db,Quote(db,MSG_ERROR_WRITING_TO_SOCKET_TXT),
									GetSocketErrorString(db,SOCKETBASE,errno));
*/
								ior->io_Error = ParErr_LineErr;
								ready = FALSE;
							}

						}
					}

					break;
					
				case GCPCMD_LIST:
					{
						struct GCPIOReq *gio = (struct GCPIOReq *)ior;
			
						if(list_printers(db, gio->gio_names, gio->gio_ids, gio->gio_size) != ERR_OK) {
							ior->io_Error = IOERR_BADLENGTH;
						}
					}
				break;
		
			}

			ReplyMsg((struct Message *)ior);

			Forbid();
		}

		Permit();
	}
	while(NOT done);

	cleanup(db);

#if 0
	#ifdef __amigaos4__
	DropInterface((struct Interface *) ISocket);
	DropInterface((struct Interface *) IUserGroup);
	#endif

	CloseLibrary(SocketBase);
	CloseLibrary(UserGroupBase);
#endif
	Forbid();

	du->du_Process = NULL;
}

/****************************************************************************/

static void ASM
prbuf(REG(d0, char c), REG(a3, char **buf_ptr))
{
	char *buf;

	buf = (*buf_ptr);
	(*buf++) = c;
	(*buf_ptr) = buf;
}

/****************************************************************************/

#define IExec db->db_IExec
#define SysBase db->db_SysBase

VOID VARARGS68K
SPrintf(struct DeviceBase *db, STRPTR PutChData, STRPTR FormatStr, ...)
{
	va_list	 args;

#ifdef __amigaos4__
	va_startlinear (args, FormatStr);
	RawDoFmt(FormatStr, va_getlinearva(args,void *), (void (*)())prbuf, &PutChData);
#else
	va_start (args, FormatStr);
	RawDoFmt(FormatStr, args, (void (*)())prbuf, &PutChData);
#endif
	va_end (args);
}

/****************************************************************************/


