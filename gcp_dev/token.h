#ifndef GCP_TOKEN_H
#define GCP_TOKEN_H 1

#include "gcp_dev/devbase.h"

enum {
	ERR_OK=0,
	ERR_NO_AUTH,
	ERR_NO_PRINTER,
	ERR_PRINT,
	ERR_NOMEM
};

int print_file(struct DeviceBase *devb, int unit, const char *filename);
int list_printers(struct DeviceBase *devb, char **names, char **ids, uint32 size);

void cleanup(struct DeviceBase *devb);
#endif
