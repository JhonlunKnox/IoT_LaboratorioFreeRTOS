# FreeRTOS Touch Sensor System — ESP32

Práctica de FreeRTOS: Tasks, Queues y Mutex aplicados a lectura de sensores touch capacitivos.

## Estructura del repositorio

```
FreeRTOS_TouchSensor/
├── FreeRTOS_TouchSensor/
│   ├── FreeRTOS_TouchSensor.ino   ← Sketch principal (importar en Arduino IDE)
│   ├── deadlock_example.h         ← Ejemplo de deadlock + solución
│   └── race_condition_example.h   ← Ejemplo condición de carrera + solución
├── PREGUNTAS_Y_RESPUESTAS.md      ← Documento con las 6 preguntas respondidas
└── README.md                      ← Este archivo
```

## Hardware requerido

- ESP32 (cualquier variante con soporte de touch capacitivo)
- Dos electrodos/pads conductores conectados a:
  - `GPIO 4` (T0) → Sensor A
  - `GPIO 15` (T3) → Sensor B

## Importar en Arduino IDE

1. Abrir Arduino IDE
2. `Archivo → Abrir` → seleccionar `FreeRTOS_TouchSensor/FreeRTOS_TouchSensor.ino`
3. Seleccionar placa: `ESP32 Dev Module`
4. Compilar y cargar

> No se requieren librerías externas: FreeRTOS viene incluido en el core de ESP32 para Arduino.

## Descripción del sistema

```
[Touch A / GPIO4] → TaskSensorA ──→ QueueA ──→ TaskSerialA ──┐
                                                               ├──→ Serial (JSON)
[Touch B /GPIO15] → TaskSensorB ──→ QueueB ──→ TaskSerialB ──┘
                                                   ↕
                                            Mutex Serial
                                       (previene acceso concurrente)
```

### Salida JSON por Serial (115200 baud)

```json
{"sensor":0,"timestamp_ms":1523,"raw_value":28,"touched":true}
{"sensor":1,"timestamp_ms":1545,"raw_value":62,"touched":false}
```

## Parámetros ajustables

| Constante | Default | Descripción |
|---|---|---|
| `TOUCH_THRESHOLD` | 40 | Umbral de detección de toque (bajar = menos sensible) |
| `QUEUE_SIZE` | 10 | Elementos máximos en cada cola |
| `SENSOR_READ_DELAY_MS` | 200 | Período de lectura de sensores (ms) |

## Conceptos FreeRTOS demostrados

- **Reutilización de funciones de tarea**: `vTaskLeerSensor()` y `vTaskEnviarSerial()` son ejecutadas por 2 tareas cada una con distintos parámetros (`void*` → cast a struct específico)
- **Colas independientes**: cada sensor tiene su propia `QueueHandle_t`
- **Timestamp en la cola**: cada `SensorData_t` incluye `timestamp_ms = millis()`
- **Mutex para Serial**: `xSerialMutex` garantiza que solo una tarea escribe a la vez
- **Política de cola llena**: dato descartado + advertencia (configurable a bloqueo)
