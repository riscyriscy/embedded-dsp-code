#include <stdio.h>
extern void initialise_monitor_handles(void);
__attribute__((constructor)) static void sh(void){ initialise_monitor_handles(); }
