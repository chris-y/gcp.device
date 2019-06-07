#ifndef DEVICES_GCP_H
#define DEVICES_GCP_H 1
#include <exec/devices.h>
#include <exec/io.h>

#define GCPCMD_LIST 0x8000

struct GCPIOReq {
	struct IORequest ioreq;
	uint32 gio_size;
	char **gio_names;
	char **gio_ids;
};

#endif
