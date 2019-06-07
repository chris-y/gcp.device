#ifndef GCP_DEVBASE_H
#define GCP_DEVBASE_H 1
#include <exec/devices.h>

struct DeviceUnit
{
	struct Unit			du_Unit;
	LONG				du_Index;
	struct Process *	du_Process;
};

/****************************************************************************/

struct DeviceBase
{
	struct Library			db_Library;
	BPTR					db_SegList;
	struct SignalSemaphore	db_Lock;
	UWORD					db_Pad;
	struct Library *		db_SysBase;
	struct Library *		db_DOSBase;
	struct Library *		db_IntuitionBase;
	struct Library *		db_UtilityBase;
	struct DeviceUnit		db_Unit[10];
	LONG					db_Counter;
#ifdef __amigaos4__
	struct ExecIFace *		db_IExec;
	struct DOSIFace  * 		db_IDOS;
	struct IntuitionIFace *	db_IIntuition;
	struct UtilityIFace *	db_IUtility;
#endif
	char *access_token;
	char *refresh_token;
	ULONG access_token_expiry;
};

/****************************************************************************/

#define SysBase			db->db_SysBase
#define DOSBase			db->db_DOSBase
#define IntuitionBase	db->db_IntuitionBase
#define UtilityBase		db->db_UtilityBase

#define IExec			db->db_IExec
#define IDOS			db->db_IDOS
#define IIntuition		db->db_IIntuition
#define IUtility		db->db_IUtility

/****************************************************************************/
#endif
