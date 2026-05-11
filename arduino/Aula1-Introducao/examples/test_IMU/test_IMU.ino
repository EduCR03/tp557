#include <Arduino_LSM9DS1.h>

// Modo atual de leitura
// 'a' = acelerômetro
// 'g' = giroscópio
// 'm' = magnetômetro
char mode = 'a';

void setup() {
  Serial.begin(9600);

  // Aguarda a abertura do Serial Monitor/Plotter
  while (!Serial);

  Serial.println("Welcome to the IMU test for the built-in IMU on the Nano 33 BLE Sense");
  Serial.println();
  Serial.println("Available commands:");
  Serial.println("a - display accelerometer readings in g's in x, y, and z directions");
  Serial.println("g - display gyroscope readings in deg/s in x, y, and z directions");
  Serial.println("m - display magnetometer readings in uT in x, y, and z directions");
  Serial.println();

  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  Serial.println("IMU initialized successfully.");
  Serial.println("Send a, g, or m to choose the sensor output.");
  Serial.println();
}

void loop() {
  // Verifica se o usuário enviou algum comando pelo Serial
  if (Serial.available() > 0) {
    char command = Serial.read();

    if (command == 'a' || command == 'g' || command == 'm') {
      mode = command;

      Serial.print("Selected mode: ");

      if (mode == 'a') {
        Serial.println("Accelerometer");
      } else if (mode == 'g') {
        Serial.println("Gyroscope");
      } else if (mode == 'm') {
        Serial.println("Magnetometer");
      }

      delay(500);
    }
  }

  float x, y, z;

  if (mode == 'a') {
    if (IMU.accelerationAvailable()) {
      IMU.readAcceleration(x, y, z);

      // Formato compatível com Serial Plotter
      Serial.print("Ax:");
      Serial.print(x);
      Serial.print("\tAy:");
      Serial.print(y);
      Serial.print("\tAz:");
      Serial.println(z);
    }
  }

  else if (mode == 'g') {
    if (IMU.gyroscopeAvailable()) {
      IMU.readGyroscope(x, y, z);

      Serial.print("Gx:");
      Serial.print(x);
      Serial.print("\tGy:");
      Serial.print(y);
      Serial.print("\tGz:");
      Serial.println(z);
    }
  }

  else if (mode == 'm') {
    if (IMU.magneticFieldAvailable()) {
      IMU.readMagneticField(x, y, z);

      Serial.print("Mx:");
      Serial.print(x);
      Serial.print("\tMy:");
      Serial.print(y);
      Serial.print("\tMz:");
      Serial.println(z);
    }
  }

  delay(100);
}