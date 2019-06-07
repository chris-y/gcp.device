/* DeviceUse.c - an example of using an Amiga device (here, serial device)    */
/*                                                                            */
/* If successful, use the serial command SDCMD_QUERY, then reverse our steps. */
/* If we encounter an error at any time, we will gracefully exit.             */

#include <stdlib.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/serial.h>
 
#include <proto/exec.h>
#include <proto/dos.h>

#include <proto/listbrowser.h>
#include <gadgets/listbrowser.h>

#include "gcp_dev/gcp.h"
#include "gcp_prefs_gui.h"

#include "GCP_rev.h"

const char UNUSED *ver = VERSTAG;

#define MAX_PRINTERS 20
#define MAX_UNITS 10

int main()
{
    struct GCPIOReq *reply;         /* for use with GetMsg             */
 
    struct MsgPort *serialMP = AllocSysObjectTags(ASOT_PORT, TAG_END);
 
    if (serialMP != NULL)
    {
        /* Create the I/O request. Note that <devices/serial.h> defines the type */
        /* of IORequest required by the serial device--an IOExtSer. Many devices */
        /* require specialized extended IO requests which start with an embedded */
        /* struct IORequest. The generic Exec and amiga.lib device IO functions  */
        /* are prototyped for IORequest, so some pointer casting is necessary.   */
 
        struct GCPIOReq *serialIO = AllocSysObjectTags(ASOT_IOREQUEST,
          ASOIOR_Size, sizeof(struct GCPIOReq),
          ASOIOR_ReplyPort, serialMP,
          TAG_END);
 
        if (serialIO != NULL)
        {
  		    ULONG i = 0;
  		    ULONG j = 0;
        	char **names = calloc(MAX_PRINTERS, sizeof(char *));
         	char **ids = calloc(MAX_PRINTERS, sizeof(char *));
			struct Node *defaults[MAX_UNITS];

        	struct List printer_list;
            NewList(&printer_list);

 			serialIO->gio_size = MAX_PRINTERS; /* TODO: free these wth FreeVec */
 			serialIO->gio_names = names;
 			serialIO->gio_ids = ids;
           
           /* Open the serial device (non-zero return value means failure here). */
            if (OpenDevice("gcp.device", 0, (struct IORequest *)serialIO, 0))
                Printf("Error: %s did not open\n", "gcp.device");
            else
            {
                /* Device is open */                         /* DoIO - demonstrates synchronous */
                serialIO->ioreq.io_Command  = GCPCMD_LIST;   /* device use, returns error or 0. */
                if (DoIO((struct IORequest *)serialIO)) {
                    Printf("Query  failed. Error - %d\n", serialIO->ioreq.io_Error);
                } else {
                    /* Print serial device status - see include file for meaning */
                    /* Note that with DoIO, the Wait and GetMsg are done by Exec */

                   char *id[MAX_UNITS];
                                       
 			     	for(j = 0; j < MAX_UNITS; j++) {
                   		id[j] = get_id(j);
               			defaults[j] = NULL;
                    }
                    
                    for(i = 0; i < MAX_PRINTERS; i++) {
                    	if((names[i] != NULL) && (ids[i] != NULL)) {
                    	//	Printf("Printer %ld: %s [%s]\n", i, names[i], ids[i]);
                    		struct Node *node = AllocListBrowserNode(1, 
                    			LBNA_UserData, ids[i],
                    			LBNA_Column, 0,
	                    			LBNCA_Text, names[i],
                    			TAG_DONE);
                    			
                    		AddTail(&printer_list, node);

 			                for(j = 0; j < MAX_UNITS; j++) {
                     			if((id[j]) && (strcmp(id[j], ids[i]) == 0)) {
                    				defaults[j] = node;
                    			}
                    		}
                    	}
 					}
 			     	for(j = 0; j < MAX_UNITS; j++) {
                   		free_id(id[j]);
                    }
 				}

                CloseDevice((struct IORequest *)serialIO);  /* Close the serial device.    */

				show_gui(&printer_list, defaults);
								 				
 				FreeListBrowserList(&printer_list);
 				
 				for(i = 0; i < MAX_PRINTERS; i++) {
                    if(names[i] != NULL) FreeVec(names[i]);
                    if(ids[i] != NULL) FreeVec(ids[i]);
				}
				
				free(names);
				free(ids);
			
            }
            FreeSysObject(ASOT_IOREQUEST, serialIO);   /* Delete the I/O request.     */
        }
        else Printf("Error: Could not create I/O request\n"); /* Inform user that the I/O    */
                                                                /* request could be created.   */
        FreeSysObject(ASOT_PORT, serialMP);          /* Delete the message port.    */
    }
    else Printf("Error: Could not create message port\n");  /* Inform user that the message*/
                                                                  /* port could not be created.  */
 
    return 0;
}
