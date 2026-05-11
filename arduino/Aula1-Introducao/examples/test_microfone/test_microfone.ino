#include <PDM.h>

// Buffer para armazenar amostras de áudio
short sampleBuffer[256];

// Número de amostras disponíveis
volatile int samplesRead = 0;

// Controla se está gravando ou não
bool recording = false;

void setup() {
  Serial.begin(115200);

  while (!Serial);

  Serial.println("Welcome to the microphone test for the built-in microphone on the Nano 33 BLE Sense");
  Serial.println();
  Serial.println("Send the command 'click' to start and stop audio recording.");
  Serial.println("Open the Serial Plotter to view the corresponding waveform.");
  Serial.println();

  // Define a função chamada quando novos dados do microfone estiverem disponíveis
  PDM.onReceive(onPDMdata);

  // Inicializa o microfone PDM
  // 1 canal, frequência de amostragem de 16 kHz
  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM!");
    while (1);
  }

  // Ajusta o ganho do microfone
  // Valores comuns: 20 a 80
  PDM.setGain(30);

  Serial.println("Microphone initialized successfully.");
  Serial.println("Type 'click' and press SEND to start.");
}

void loop() {
  // Verifica se algum comando foi enviado pelo Serial Monitor/Plotter
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "click") {
      recording = !recording;

      if (recording) {
        Serial.println("Recording started.");
      } else {
        Serial.println("Recording stopped.");
      }
    }
  }

  // Se estiver gravando e houver amostras disponíveis
  if (recording && samplesRead > 0) {
    for (int i = 0; i < samplesRead; i++) {
      // Envia as amostras para o Serial Plotter
      Serial.println(sampleBuffer[i]);
    }

    // Zera o contador de amostras
    samplesRead = 0;
  }
}

// Função chamada automaticamente quando o microfone recebe novos dados
void onPDMdata() {
  // Verifica quantos bytes estão disponíveis
  int bytesAvailable = PDM.available();

  // Lê os dados do microfone
  PDM.read(sampleBuffer, bytesAvailable);

  // Cada amostra ocupa 2 bytes, pois é do tipo short
  samplesRead = bytesAvailable / 2;
}