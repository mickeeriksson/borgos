#ifndef __MODULE_H__
#define __MODULE_H__

#include "log.h"
#include "types.h"

typedef struct module_entry {
    adr_t mod_start;
    adr_t mod_end;
    char mod_name[32];
} module_entry_t;


extern int module_add(adr_t mod_start,adr_t mod_end,char* mod_name);
extern module_entry_t* module_get(char* mod_name);
extern void module_debuglist(void);

#endif