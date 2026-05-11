#include <TinyMLShield.h>

bool liveMode = false;
bool captureRequested = false;

// QCIF = 176 x 144 pixels
// RGB565 = 2 bytes por pixel
byte image[176 * 144 * 2];

int bytesPerFrame = 0;

void printMenu() {
  Serial.println();
  Serial.println("Welcome to the OV7675 camera test");
  Serial.println();
  Serial.println("Available commands:");
  Serial.println("single  - take a single image and print hexadecimal pixels");
  Serial.println("live    - stream raw camera bytes over serial");
  Serial.println("capture - capture one image in single mode");
  Serial.println();
  Serial.println("In single mode, you can also press the TinyML Shield button.");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Inicializa o TinyML Shield
  initializeShield();

  // Inicializa a câmera OV7675
  if (!Camera.begin(QCIF, RGB565, 1, OV7675)) {
    Serial.println("Failed to initialize camera!");
    while (1);
  }

  bytesPerFrame = Camera.width() * Camera.height() * Camera.bytesPerPixel();

  Serial.println("Camera initialized successfully.");
  Serial.print("Width: ");
  Serial.println(Camera.width());
  Serial.print("Height: ");
  Serial.println(Camera.height());
  Serial.print("Bytes per pixel: ");
  Serial.println(Camera.bytesPerPixel());
  Serial.print("Bytes per frame: ");
  Serial.println(bytesPerFrame);

  printMenu();
}

void loop() {
  // Botão físico da TinyML Shield
  if (readShieldButton()) {
    if (!liveMode) {
      captureRequested = true;
    }
  }

  // Comandos pelo Serial Monitor
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();

    if (command == "single") {
      liveMode = false;
      Serial.println();
      Serial.println("Camera in single mode.");
      Serial.println("Type 'capture' or press the shield button to capture an image.");
    }

    else if (command == "capture") {
      if (!liveMode) {
        captureRequested = true;
      } else {
        Serial.println();
        Serial.println("Camera is in live mode. Type 'single' first.");
      }
    }

    else if (command == "live") {
      Serial.println();
      Serial.println("Raw image data will begin streaming in 5 seconds...");
      Serial.println("Warning: raw bytes will not look readable in Serial Monitor.");
      delay(5000);
      liveMode = true;
    }

    else if (command.length() > 0) {
      Serial.println();
      Serial.println("Unknown command.");
      printMenu();
    }
  }

  // Modo live: envia bytes crus continuamente
  if (liveMode) {
    Camera.readFrame(image);
    Serial.write(image, bytesPerFrame);
  }

  // Modo single: captura uma imagem e imprime pixels em hexadecimal
  if (captureRequested) {
    captureRequested = false;

    Serial.println();
    Serial.println("Capturing image...");
    Camera.readFrame(image);

    Serial.println("Image data will be printed out in 3 seconds...");
    delay(3000);

    for (int i = 0; i < bytesPerFrame - 1; i += 2) {
      uint16_t pixel = ((uint16_t)image[i + 1] << 8) | image[i];

      Serial.print("0x");

      if (pixel < 0x1000) Serial.print("0");
      if (pixel < 0x0100) Serial.print("0");
      if (pixel < 0x0010) Serial.print("0");

      Serial.print(pixel, HEX);

      if (i != bytesPerFrame - 2) {
        Serial.print(", ");
      }
    }

    Serial.println();
    Serial.println("Capture complete.");
  }
}
