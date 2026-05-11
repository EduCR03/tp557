#include <PDM.h>

// Buffer para armazenar amostras de audio
short sampleBuffer[256];

// Numero de amostras disponiveis
volatile int samplesRead = 0;

// Controla se esta gravando ou nao
bool recording = false;

void onPDMdata();

void setup()
{
    Serial.begin(115200);

    while (!Serial);

    Serial.println("Welcome to the microphone test for the built-in microphone on the Nano 33 BLE Sense");
    Serial.println();
    Serial.println("Send the command 'click' to start and stop audio recording.");
    Serial.println("Open the Serial Plotter to view the corresponding waveform.");
    Serial.println();

    // Chama a funcao quando novos dados estiverem disponiveis
    PDM.onReceive(onPDMdata);

    // Inicializa microfone PDM com 1 canal e 16 kHz
    if (!PDM.begin(1, 16000))
    {
        Serial.println("Failed to start PDM!");
        while (1);
    }

    // Ajusta o ganho do microfone
    PDM.setGain(30);

    Serial.println("Microphone initialized successfully.");
    Serial.println("Type 'click' and press SEND to start.");
}

void loop()
{
    // Verifica comando do Serial Monitor
    if (Serial.available() > 0)
    {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command == "click")
        {
            recording = !recording;

            if (recording)
            {
                Serial.println("Recording started.");
            }
            else
            {
                Serial.println("Recording stopped.");
            }
        }
    }

    // Envia amostras para o Serial Plotter
    if (recording && samplesRead > 0)
    {
        for (int i = 0; i < samplesRead; i++)
        {
            Serial.println(sampleBuffer[i]);
        }

        samplesRead = 0;
    }
}

void onPDMdata()
{
    // Verifica quantos bytes estao disponiveis
    int bytesAvailable = PDM.available();

    // Le os dados do microfone
    PDM.read(sampleBuffer, bytesAvailable);

    // Cada amostra ocupa 2 bytes
    samplesRead = bytesAvailable / 2;
}
