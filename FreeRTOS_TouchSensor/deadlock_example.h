/*
 * ============================================================
 *  EJEMPLO: Deadlock en FreeRTOS (Arduino/ESP32)
 *  Pregunta: ¿Qué es un deadlock?
 * ============================================================
 *
 *  DESCRIPCIÓN DEL DEADLOCK:
 *
 *  Tarea A toma MutexX → intenta tomar MutexY → BLOQUEADA (Y está en B)
 *  Tarea B toma MutexY → intenta tomar MutexX → BLOQUEADA (X está en A)
 *  → NINGUNA puede avanzar → DEADLOCK
 *
 *  SOLUCIÓN: Adquirir los mutex siempre en el mismo orden (jerarquía).
 * ============================================================
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

SemaphoreHandle_t xMutexX = NULL;
SemaphoreHandle_t xMutexY = NULL;

// ── Tarea A: toma X primero, luego Y ──────────────────────────
void vTareaA_Deadlock(void* pv) {
  for (;;) {
    Serial.println("[Tarea A] Intentando tomar MutexX...");
    xSemaphoreTake(xMutexX, portMAX_DELAY);
    Serial.println("[Tarea A] MutexX tomado. Esperando 100ms...");

    vTaskDelay(pdMS_TO_TICKS(100)); // Simula trabajo

    Serial.println("[Tarea A] Intentando tomar MutexY...");
    // *** AQUÍ SE BLOQUEA si la Tarea B ya tiene MutexY ***
    xSemaphoreTake(xMutexY, portMAX_DELAY);

    Serial.println("[Tarea A] Sección crítica ejecutada");
    xSemaphoreGive(xMutexY);
    xSemaphoreGive(xMutexX);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// ── Tarea B: toma Y primero, luego X (orden INVERTIDO → deadlock) ──
void vTareaB_Deadlock(void* pv) {
  for (;;) {
    Serial.println("[Tarea B] Intentando tomar MutexY...");
    xSemaphoreTake(xMutexY, portMAX_DELAY);
    Serial.println("[Tarea B] MutexY tomado. Esperando 100ms...");

    vTaskDelay(pdMS_TO_TICKS(100)); // Simula trabajo

    Serial.println("[Tarea B] Intentando tomar MutexX...");
    // *** AQUÍ SE BLOQUEA si la Tarea A ya tiene MutexX ***
    xSemaphoreTake(xMutexX, portMAX_DELAY);

    Serial.println("[Tarea B] Sección crítica ejecutada");
    xSemaphoreGive(xMutexX);
    xSemaphoreGive(xMutexY);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// ════════════════════════════════════════════════════════════
//  VERSIÓN CORREGIDA: Ambas tareas adquieren en el mismo orden
// ════════════════════════════════════════════════════════════

void vTareaA_Corregida(void* pv) {
  for (;;) {
    xSemaphoreTake(xMutexX, portMAX_DELAY);  // Siempre X primero
    xSemaphoreTake(xMutexY, portMAX_DELAY);  // Luego Y

    Serial.println("[Tarea A CORR] Sección crítica con X e Y");

    xSemaphoreGive(xMutexY);  // Liberar en orden inverso
    xSemaphoreGive(xMutexX);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void vTareaB_Corregida(void* pv) {
  for (;;) {
    xSemaphoreTake(xMutexX, portMAX_DELAY);  // Siempre X primero (igual que A)
    xSemaphoreTake(xMutexY, portMAX_DELAY);  // Luego Y

    Serial.println("[Tarea B CORR] Sección crítica con X e Y");

    xSemaphoreGive(xMutexY);
    xSemaphoreGive(xMutexX);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// ── Para probar el deadlock: descomentar el bloque "DEADLOCK"
//    y comentar el bloque "CORREGIDO"

void setup_deadlock_example() {
  Serial.begin(115200);
  delay(1000);

  xMutexX = xSemaphoreCreateMutex();
  xMutexY = xSemaphoreCreateMutex();

  // ---- BLOQUE DEADLOCK (genera deadlock) ----
  xTaskCreate(vTareaA_Deadlock, "TareaA", 2048, NULL, 1, NULL);
  xTaskCreate(vTareaB_Deadlock, "TareaB", 2048, NULL, 1, NULL);

  // ---- BLOQUE CORREGIDO (sin deadlock) ----
  // xTaskCreate(vTareaA_Corregida, "TareaA", 2048, NULL, 1, NULL);
  // xTaskCreate(vTareaB_Corregida, "TareaB", 2048, NULL, 1, NULL);
}
