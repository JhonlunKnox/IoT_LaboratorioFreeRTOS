/*
 * ============================================================
 *  EJEMPLO: Condición de carrera (Race Condition)
 *  Sin mutex → datos inconsistentes
 *  Con mutex → datos consistentes
 * ============================================================
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// ─────────────────────────────────────────────
//  Recurso compartido (variable global)
// ─────────────────────────────────────────────
volatile uint32_t contador_sin_mutex = 0;
volatile uint32_t contador_con_mutex = 0;

SemaphoreHandle_t xMutexContador = NULL;

// ─────────────────────────────────────────────
//  SIN MUTEX: Dos tareas incrementan el contador
//  La secuencia: leer → incrementar → escribir
//  puede ser interrumpida entre pasos → inconsistencia
// ─────────────────────────────────────────────
void vIncrementar_SinMutex(void* pv) {
  for (;;) {
    for (int i = 0; i < 1000; i++) {
      // Simula Read-Modify-Write no atómico
      uint32_t tmp = contador_sin_mutex;   // 1. LEER
      vTaskDelay(1);                        // 2. Aquí puede ocurrir un context switch
      contador_sin_mutex = tmp + 1;         // 3. ESCRIBIR (basado en valor viejo)
    }
    Serial.printf("[SIN MUTEX] Valor esperado: ~2000  |  Valor real: %lu\n",
                  (unsigned long) contador_sin_mutex);
    contador_sin_mutex = 0;
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// ─────────────────────────────────────────────
//  CON MUTEX: Operación protegida
// ─────────────────────────────────────────────
void vIncrementar_ConMutex(void* pv) {
  for (;;) {
    for (int i = 0; i < 1000; i++) {
      // Tomar mutex antes de la sección crítica
      xSemaphoreTake(xMutexContador, portMAX_DELAY);

      uint32_t tmp = contador_con_mutex;
      vTaskDelay(1);                         // El context switch es seguro aquí
      contador_con_mutex = tmp + 1;

      xSemaphoreGive(xMutexContador);        // Liberar mutex
    }
    Serial.printf("[CON MUTEX]  Valor esperado: ~2000  |  Valor real: %lu\n",
                  (unsigned long) contador_con_mutex);
    contador_con_mutex = 0;
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void setup_race_condition_example() {
  Serial.begin(115200);
  xMutexContador = xSemaphoreCreateMutex();

  // Crear dos pares de tareas para comparar comportamiento
  xTaskCreate(vIncrementar_SinMutex, "SinMutexA", 2048, NULL, 1, NULL);
  xTaskCreate(vIncrementar_SinMutex, "SinMutexB", 2048, NULL, 1, NULL);

  xTaskCreate(vIncrementar_ConMutex, "ConMutexA",  2048, NULL, 1, NULL);
  xTaskCreate(vIncrementar_ConMutex, "ConMutexB",  2048, NULL, 1, NULL);
}
