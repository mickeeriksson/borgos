#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

extern void scheduler_init(void);
extern void scheduler_schedule(void);

extern void scheduler_enqueue(proc_t* p);

#endif