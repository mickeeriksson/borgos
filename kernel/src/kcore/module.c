#include "module.h"
#include "log.h"
#include "error.h"
#include <string.h>

module_entry_t mod_table[16];
int modules_count=0;

int module_add(adr_t mod_start,adr_t mod_end,char* mod_name) {
    module_entry_t *mod = &mod_table[modules_count];
    mod->mod_start = mod_start;
    mod->mod_end = mod_end;
    strlcpy(&mod->mod_name[0],mod_name,32);
    log_msg("ADD MODULE : %s\n",mod->mod_name);
    log_msg("  mod_start = %#lx\n",mod->mod_start);
    log_msg("  mod_end = %#lx\n",mod->mod_end);
    modules_count++;
    return 0;
}
extern module_entry_t* module_get(char* mod_name) {
    for (int i=0;i<modules_count;i++) {
        module_entry_t *mod = &mod_table[i];
        if (strcmp(mod->mod_name,mod_name) == 0) {
            return mod;
        }
    }
    return NULL;
}

void module_debuglist(void) {
   for (int i=0;i<modules_count;i++) {
       module_entry_t *mod = &mod_table[i];
       log_msg("MODULE %d : %s\n",i,mod->mod_name);
       log_msg("  mod_start = %#lx\n",mod->mod_start);
       log_msg("  mod_end = %#lx\n",mod->mod_end);
   }
}
