#ifndef GCP_PREFS_GUI_H
#define GCP_PREFS_GUI_H 1

struct List;

void show_gui(struct List *window_list, struct Node **defaults);

void free_id(char *id);
char *get_id(int unit);

#endif
