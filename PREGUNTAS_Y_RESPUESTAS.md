# Preguntas FreeRTOS — Respuestas y Ejemplos de Código

---

## 1. ¿Cómo se ejecutan tareas de FreeRTOS con la misma función pero distintos parámetros?

`xTaskCreate()` recibe un puntero `void*` como último argumento `pvParameters`. Se puede pasar cualquier estructura como parámetro, siempre y cuando su ciclo de vida supere al de la tarea (usar `static` o memoria dinámica).

**Ejemplo del proyecto:**
```c
// Misma función vTaskLeerSensor ejecutada por dos tareas con distintos pines y colas
static SensorTaskParams_t paramsA = { .sensor_id=0, .touch_pin=T0, .queue=xQueueA };
static SensorTaskParams_t paramsB = { .sensor_id=1, .touch_pin=T3, .queue=xQueueB };

xTaskCreate(vTaskLeerSensor, "SensorA", 2048, &paramsA, 2, NULL);
xTaskCreate(vTaskLeerSensor, "SensorB", 2048, &paramsB, 2, NULL);
```

Dentro de la función, se hace el cast del parámetro genérico al tipo específico (ver pregunta 2).

---

## 2. ¿Cuál es el tipo de dato que recibe una tarea de FreeRTOS? ¿Cómo se convierte al tipo específico?

Las tareas de FreeRTOS reciben un puntero genérico `void*`. Para recuperar el tipo original se hace un **cast explícito**:

```c
void vTaskLeerSensor(void* pvParameters) {
    // Cast de void* al tipo específico
    SensorTaskParams_t* params = (SensorTaskParams_t*) pvParameters;

    // Ahora se puede acceder a los campos
    uint16_t valor = touchRead(params->touch_pin);
    xQueueSend(params->queue, &dato, 0);
}
```

El uso de `void*` es deliberado: permite que el scheduler de FreeRTOS sea agnóstico al tipo de dato que maneja cada tarea, manteniendo la API genérica y reutilizable.

---

## 3. ¿Qué pasa cuando una cola se llena y una tarea quiere insertar nuevos elementos?

`xQueueSend(queue, &dato, xTicksToWait)` tiene tres comportamientos según el tercer argumento:

| Valor de `xTicksToWait` | Comportamiento si la cola está llena |
|---|---|
| `0` | Retorna inmediatamente con `errQUEUE_FULL` |
| `pdMS_TO_TICKS(N)` | Bloquea la tarea hasta N milisegundos; si sigue llena, retorna `errQUEUE_FULL` |
| `portMAX_DELAY` | Bloquea indefinidamente hasta que haya espacio disponible |

En el proyecto se usa `0` para los sensores (se descarta el dato y se imprime advertencia), evitando bloquear la tarea de lectura que debe ser periódica.

---

## 4. ¿Es posible que varias tareas lean y escriban a la misma cola?

**Sí.** Las colas de FreeRTOS son intrínsecamente thread-safe. Internamente usan un mecanismo de suspensión de tareas y secciones críticas para garantizar que múltiples productores y múltiples consumidores puedan acceder a la misma cola sin corrupción de datos.

```
Cola compartida:
  Productor 1 ──┐
  Productor 2 ──┼──→ [Cola] ──┬──→ Consumidor 1
  Productor N ──┘             └──→ Consumidor 2
```

FreeRTOS maneja internamente el acceso concurrente a la cola. No se necesita un mutex adicional para proteger la cola en sí, aunque el orden de entrega entre múltiples consumidores depende de sus prioridades.

---

## 5. ¿Qué es un deadlock? Ejemplo de código y cómo evitarlo.

Un **deadlock** ocurre cuando dos o más tareas se bloquean mutuamente esperando recursos que la otra tarea retiene, y ninguna puede avanzar.

**Condiciones necesarias (Coffman, 1971):**
1. Exclusión mutua (los recursos no se comparten)
2. Retención y espera (una tarea retiene un recurso mientras espera otro)
3. Sin desalojo (no se pueden quitar recursos forzosamente)
4. Espera circular (A espera a B que espera a A)

### Código que genera deadlock

```c
SemaphoreHandle_t xMutexX, xMutexY;

// Tarea A: toma X, luego Y
void vTareaA(void* pv) {
    xSemaphoreTake(xMutexX, portMAX_DELAY);  // Toma X
    vTaskDelay(pdMS_TO_TICKS(100));           // context switch posible aquí
    xSemaphoreTake(xMutexY, portMAX_DELAY);  // BLOQUEADA: B tiene Y
    // nunca llega aquí...
    xSemaphoreGive(xMutexY);
    xSemaphoreGive(xMutexX);
}

// Tarea B: toma Y, luego X (orden INVERTIDO)
void vTareaB(void* pv) {
    xSemaphoreTake(xMutexY, portMAX_DELAY);  // Toma Y
    vTaskDelay(pdMS_TO_TICKS(100));           // context switch posible aquí
    xSemaphoreTake(xMutexX, portMAX_DELAY);  // BLOQUEADA: A tiene X
    // nunca llega aquí...
    xSemaphoreGive(xMutexX);
    xSemaphoreGive(xMutexY);
}
```

**Secuencia del deadlock:**
```
t=0:   A toma X      | B toma Y
t=100: A espera Y ⏸  | B espera X ⏸
t=∞:   DEADLOCK — ninguna avanza
```

### Solución: Jerarquía de adquisición (Lock Ordering)

```c
// SOLUCIÓN: Ambas tareas adquieren SIEMPRE en el mismo orden (X antes que Y)

void vTareaA_Corregida(void* pv) {
    xSemaphoreTake(xMutexX, portMAX_DELAY);  // X primero
    xSemaphoreTake(xMutexY, portMAX_DELAY);  // Y después
    // sección crítica...
    xSemaphoreGive(xMutexY);
    xSemaphoreGive(xMutexX);
}

void vTareaB_Corregida(void* pv) {
    xSemaphoreTake(xMutexX, portMAX_DELAY);  // X primero (igual que A)
    xSemaphoreTake(xMutexY, portMAX_DELAY);  // Y después
    // sección crítica...
    xSemaphoreGive(xMutexY);
    xSemaphoreGive(xMutexX);
}
```

**Otras estrategias anti-deadlock:**
- Usar `xSemaphoreTake` con timeout (no `portMAX_DELAY`) y liberar si falla
- Usar un único mutex por recurso siempre que sea posible
- Diseñar el sistema para que nunca haya necesidad de dos mutex simultáneos

---

## 6. Condición de carrera: sin mutex vs con mutex

### Sin mutex — datos inconsistentes

```c
volatile uint32_t contador = 0;

void vIncrementar_SinMutex(void* pv) {
    for (;;) {
        for (int i = 0; i < 1000; i++) {
            uint32_t tmp = contador;   // 1. LEER  ← context switch puede ocurrir aquí
            vTaskDelay(1);
            contador = tmp + 1;        // 2. ESCRIBIR (basado en valor posiblemente viejo)
        }
        // Resultado esperado: 2000 (dos tareas × 1000)
        // Resultado real:  ~1000-1300 (datos corruptos por race condition)
        Serial.printf("Sin mutex: %lu\n", contador);
        contador = 0;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

**¿Por qué falla?** La operación leer→incrementar→escribir no es atómica. Si la Tarea A lee `contador=500`, se produce un context switch, la Tarea B incrementa a `501`, y luego A escribe `501` (perdiendo el incremento de B).

### Con mutex — datos consistentes

```c
SemaphoreHandle_t xMutexContador;
volatile uint32_t contador = 0;

void vIncrementar_ConMutex(void* pv) {
    for (;;) {
        for (int i = 0; i < 1000; i++) {
            xSemaphoreTake(xMutexContador, portMAX_DELAY);
            uint32_t tmp = contador;
            vTaskDelay(1);
            contador = tmp + 1;
            xSemaphoreGive(xMutexContador);
        }
        // Resultado: siempre 2000 (correcto)
        Serial.printf("Con mutex: %lu\n", contador);
        contador = 0;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

**Resultado comparativo:**
```
Sin mutex  — esperado: 2000  | real: 1247  ← inconsistente
Con mutex  — esperado: 2000  | real: 2000  ← consistente
```

---

## Arquitectura hipotética del proyecto final

*(Ejemplo: sistema de monitoreo ambiental con ESP32)*

```
Tareas (Tasks):
  vTaskLeerTemp    → lee sensor DHT22 cada 1s → QueueTemp
  vTaskLeerHumedad → lee sensor DHT22 cada 1s → QueueHumedad  
  vTaskLeerCO2     → lee sensor MQ-135 → QueueCO2
  vTaskEnviarWiFi  → toma de las 3 colas → publica MQTT (con mutex WiFi)
  vTaskMostrarLCD  → toma de las 3 colas → actualiza pantalla (con mutex I2C)
  vTaskAlarma      → monitorea umbrales → controla buzzer/LED

Colas (Queues):
  QueueTemp     → entre vTaskLeerTemp y consumidores
  QueueHumedad  → entre vTaskLeerHumedad y consumidores
  QueueCO2      → entre vTaskLeerCO2 y consumidores

Mutex/Semáforos:
  xMutexWiFi  → evita acceso concurrente al stack WiFi/MQTT
  xMutexI2C   → evita colisiones en el bus I2C compartido (LCD + otros sensores)
  xSemAlarma  → semáforo binario para señalizar evento de alarma
```

Los conceptos de tareas, colas y mutex permiten desacoplar la lectura de sensores de la transmisión de datos, logrando un sistema reactivo y sin bloqueos innecesarios.
