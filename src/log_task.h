// funciones.h
#ifndef LOG_TASK_H
#define LOG_TASK_H

#include "task.h"
#include <stdio.h>

void InitLogFile(void);
void CloseLogFile(void);

void LogTaskSwitchIn(TaskHandle_t xTask);
void LogTaskSwitchOut(TaskHandle_t xTask);

#endif
