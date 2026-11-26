#ifndef __USER_FREERTOS_CONFIG_H
#define __USER_FREERTOS_CONFIG_H

void lv_freertos_task_switch_in(const char * name);
void lv_freertos_task_switch_out(void);

#define traceTASK_SWITCHED_IN()   lv_freertos_task_switch_in(pxCurrentTCB->pcTaskName);
#define traceTASK_SWITCHED_OUT()  lv_freertos_task_switch_out();

#endif
