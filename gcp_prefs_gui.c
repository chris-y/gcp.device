#include <stdlib.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include <proto/button.h>
#include <proto/label.h>
#include <proto/layout.h>
#include <proto/listbrowser.h>
#include <proto/chooser.h>
#include <proto/window.h>
#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/chooser.h>
#include <gadgets/layout.h>
#include <gadgets/listbrowser.h>
#include <images/label.h>

#include <reaction/reaction_macros.h>

enum {
	OID_MAIN = 0,
	OID_PRINTERS,
	OID_UNIT,
	OID_SAVE,
	OID_USE,
	OID_CANCEL,
	OID_LAST
};

void free_id(char *id)
{
	free(id);
}

char *get_id(int unit)
{
	char *printer_id = malloc(100);
	int res;

	char *envvar = ASPrintf("gcp.device/%ld", unit);
	
	res = GetVar(envvar, printer_id, 100, GVF_GLOBAL_ONLY);
	if(res == -1) return NULL;
	
	FreeVec(envvar);

	return printer_id;
}

static void write_prefs(struct Node **defaults, BOOL save)
{
	int j;
	char *id;
	char *envvar;
	ULONG flags = GVF_GLOBAL_ONLY;
	
	if(save == TRUE) flags |= GVF_SAVE_VAR;
	
	for(j = 0; j < 10; j++) {
		if(defaults[j] != NULL) {
			GetListBrowserNodeAttrs(defaults[j], LBNA_UserData, &id, TAG_DONE);
			envvar = ASPrintf("gcp.device/%ld", j);
			SetVar(envvar, id, -1, flags);
		}
	}
}

void show_gui(struct List *printer_list, struct Node **defaults)
{
	Object *objects[OID_LAST];
	int unit = 0;
	const char *units[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", NULL};

	objects[OID_MAIN] = WindowObject,
			WA_ScreenTitle, "GCP Prefs   (c) 2017 Unsatisfactory Software",
			WA_Title, "GCP Prefs",
			WA_Activate, TRUE,
			WA_DepthGadget, TRUE,
			WA_DragBar, TRUE,
			WA_CloseGadget, TRUE,
			WA_SizeGadget, TRUE,
			WA_SmartRefresh, TRUE,
			WINDOW_PopupGadget, TRUE,
			WINDOW_UniqueID, "GCP",
			WINDOW_Position, WPOS_CENTERSCREEN,
			WINDOW_ParentGroup, VGroupObject,
				LAYOUT_SpaceOuter, TRUE,
				LAYOUT_AddChild, objects[OID_UNIT] = ChooserObject,
					GA_ID, OID_UNIT,
					GA_RelVerify, TRUE,
					GA_TabCycle, TRUE,
					CHOOSER_LabelArray, units,
					CHOOSER_PopUp,TRUE,
				ChooserEnd,
				CHILD_Label, LabelObject,
					LABEL_Text, "Unit",
				LabelEnd,
				LAYOUT_AddChild, objects[OID_PRINTERS] = ListBrowserObject,
					GA_ID, OID_PRINTERS,
					GA_RelVerify, TRUE,
					LISTBROWSER_Labels, printer_list,
					LISTBROWSER_ShowSelected, TRUE,
					LISTBROWSER_SelectedNode, defaults[0],
				ListBrowserEnd,
				LAYOUT_AddChild, HLayoutObject,
					LAYOUT_AddChild, objects[OID_SAVE] = ButtonObject,
						GA_ID, OID_SAVE,
						GA_Text, "Save",
						GA_RelVerify, TRUE,
					ButtonEnd,
					LAYOUT_AddChild, objects[OID_USE] = ButtonObject,
						GA_ID, OID_USE,
						GA_Text, "Use",
						GA_RelVerify, TRUE,
					ButtonEnd,
					LAYOUT_AddChild, objects[OID_CANCEL] = ButtonObject,
						GA_ID, OID_CANCEL,
						GA_Text, "Cancel",
						GA_RelVerify, TRUE,
					ButtonEnd,
				LayoutEnd,
				CHILD_WeightedHeight, 0,
			LayoutEnd,
			CHILD_WeightedHeight, 0,
		WindowEnd;

	if (objects[OID_MAIN]) {
		ULONG wait, signal = 0;
		ULONG done = FALSE;
		ULONG result;
		UWORD code;
		struct Window *win;

		if ((win = (struct Window *) RA_OpenWindow(objects[OID_MAIN]))) {
			GetAttr(WINDOW_SigMask, objects[OID_MAIN], &signal);

			while (!done) {
				wait = Wait(signal);
				
				while ( (result = RA_HandleInput(objects[OID_MAIN], &code) ) != WMHI_LASTMSG ) {
					switch (result & WMHI_CLASSMASK) {
						case WMHI_CLOSEWINDOW:
							done = TRUE;
						break;
						
						case WMHI_GADGETUP:
							switch (result & WMHI_GADGETMASK)
							{
								case OID_UNIT:
									unit = code;
									
									if(defaults[unit] != NULL) {
										RefreshSetGadgetAttrs(objects[OID_PRINTERS], win, NULL,
											LISTBROWSER_SelectedNode, defaults[unit], TAG_DONE);
									} else {
										RefreshSetGadgetAttrs(objects[OID_PRINTERS], win, NULL,
											LISTBROWSER_Selected, -1, TAG_DONE);
									}
								break;
										
								case OID_PRINTERS:
									GetAttr(LISTBROWSER_SelectedNode, objects[OID_PRINTERS], &defaults[unit]);
								break;
								
								case OID_SAVE:
									write_prefs(defaults, TRUE);
									done = TRUE;
								break;
								
								case OID_USE:
									write_prefs(defaults, FALSE);
									done = TRUE;
								break;
								
								case OID_CANCEL:
									done = TRUE;
								break;
								
								default:
								break;
							}
						break;
						
						default:
						break;
					}
				}
			}
			RA_CloseWindow(objects[OID_MAIN]);
		}
	}
}
