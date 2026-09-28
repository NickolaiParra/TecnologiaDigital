// 1. Definicion de pines
#define BUTTON_1_PIN 15
#define BUTTON_2_PIN 4
#define LED_1_PIN    16
#define LED_2_PIN    17

// 2. Variables globales compartidas (reemplazan los bits del Event Group)
bool button1Pressed = false;
bool button2Pressed = false;

// Mutex que protege el acceso a las variables globales
SemaphoreHandle_t xMutex;

// Prototipos
void taskReadButtons(void *pvParameters);
void taskControlLEDs(void *pvParameters);

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  pinMode(LED_1_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);
  digitalWrite(LED_1_PIN, LOW);
  digitalWrite(LED_2_PIN, LOW);

  // 3. Crear el mutex
  xMutex = xSemaphoreCreateMutex();

  if (xMutex != NULL) {
    Serial.println("Mutex created successfully.");

    xTaskCreatePinnedToCore(taskReadButtons, "Read Buttons", 2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskControlLEDs, "Control LEDs", 2048, NULL, 2, NULL, 1);
  } else {
    Serial.println("Failed to create Mutex.");
  }
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}

/* ---------------- TAREAS ---------------- */

void taskReadButtons(void *pvParameters) {
  (void) pvParameters;

  bool lastState1 = HIGH;
  bool lastState2 = HIGH;

  for (;;) {
    bool currentState1 = digitalRead(BUTTON_1_PIN);
    bool currentState2 = digitalRead(BUTTON_2_PIN);

    // Flanco de bajada del boton 1
    if (currentState1 == LOW && lastState1 == HIGH) {
      Serial.println("-> Button 1 pressed!");

      if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
        button1Pressed = true;          // Seccion critica
        xSemaphoreGive(xMutex);
      }
      vTaskDelay(pdMS_TO_TICKS(150));   // Antirrebote (fuera del mutex)
    }

    // Flanco de bajada del boton 2
    if (currentState2 == LOW && lastState2 == HIGH) {
      Serial.println("-> Button 2 pressed!");

      if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
        button2Pressed = true;          // Seccion critica
        xSemaphoreGive(xMutex);
      }
      vTaskDelay(pdMS_TO_TICKS(150));
    }

    lastState1 = currentState1;
    lastState2 = currentState2;

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void taskControlLEDs(void *pvParameters) {
  (void) pvParameters;

  Serial.println("TaskLEDs: Waiting for BOTH buttons to be pressed...");

  for (;;) {
    bool bothPressed = false;

    // Seccion critica: leer y limpiar las banderas
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      if (button1Pressed && button2Pressed) {
        bothPressed = true;
        button1Pressed = false;   // Equivale al auto-clear (pdTRUE)
        button2Pressed = false;
      }
      xSemaphoreGive(xMutex);
    }

    if (bothPressed) {
      Serial.println("Sync Successful! Turning LEDs ON...");
      digitalWrite(LED_1_PIN, HIGH);
      digitalWrite(LED_2_PIN, HIGH);
      vTaskDelay(pdMS_TO_TICKS(2000));
      digitalWrite(LED_1_PIN, LOW);
      digitalWrite(LED_2_PIN, LOW);

      Serial.println("TaskLEDs: Waiting for BOTH buttons to be pressed...");
    }

    // Polling: ceder la CPU 20 ms antes de volver a revisar
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}