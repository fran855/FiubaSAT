#include "FreeRTOS.h"
#include "task.h"
#include "log_task.h"
#include <stdio.h>

static FILE *logFile = NULL;

void InitLogFile(void) {
    logFile = fopen("task_log.txt", "w");
    if (logFile == NULL) {
        // Manejo de error si no se puede abrir el archivo
        printf("Error al abrir el archivo de log.\n");
    }
}

void CloseLogFile(void) {
    if (logFile != NULL) {
        fclose(logFile);
    }
}

void LogTaskSwitchIn(TaskHandle_t xTask) {
    if (logFile != NULL) {
        fprintf(logFile, "Task Switched In: %s\n", pcTaskGetName(xTask));
        fflush(logFile);
    }
}

void LogTaskSwitchOut(TaskHandle_t xTask) {
    if (logFile != NULL) {
        fprintf(logFile, "Task Switched Out: %s\n", pcTaskGetName(xTask));
        fflush(logFile);
    }
}
