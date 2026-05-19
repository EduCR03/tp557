#include <Aula2-Classificacao_Movimento_inferencing.h>
#include <Arduino_LSM9DS1.h>

// Converte aceleracao de g para m/s2
#define CONVERT_G_TO_MS2 9.80665f

// Limite usado pelo modelo treinado
#define MAX_ACCEPTED_RANGE 2.0f

// Modo atual de leitura
// 'a' = acelerometro
// 'g' = giroscopio
// 'm' = magnetometro
// 'i' = inferencia do modelo
char mode = 'a';

// Buffer com as amostras usadas pelo Edge Impulse
float imu_buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};

float get_sign(float number)
{
    // Retorna o sinal do numero
    if (number >= 0.0f)
    {
        return 1.0f;
    }
    else
    {
        return -1.0f;
    }
}

void print_classification(ei_impulse_result_t result)
{
    // Comeca usando o primeiro label como melhor
    int best_index = 0;
    float best_score = result.classification[0].value;

    // Procura o label com maior pontuacao
    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++)
    {
        // Use estas linhas se quiser ver todos os scores
        // Serial.print(result.classification[i].label);
        // Serial.print(": ");
        // Serial.println(result.classification[i].value, 3);

        // Atualiza o melhor label
        if (result.classification[i].value > best_score)
        {
            best_index = i;
            best_score = result.classification[i].value;
        }
    }

    // Mostra o resultado final
    Serial.print("Best label: ");
    Serial.println(result.classification[best_index].label);
}

void run_inference()
{
    Serial.println("Sampling movement...");

    // Coleta a quantidade de amostras esperada pelo modelo
    for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 3)
    {
        // Mantem o intervalo usado no treino
        uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);

        // Espera uma nova leitura do acelerometro
        while (!IMU.accelerationAvailable());

        // Le os 3 eixos do acelerometro
        IMU.readAcceleration(imu_buffer[ix], imu_buffer[ix + 1], imu_buffer[ix + 2]);

        // Ajusta cada eixo antes de enviar para o modelo
        for (int i = 0; i < 3; i++)
        {
            // Limita valores muito altos
            if (fabs(imu_buffer[ix + i]) > MAX_ACCEPTED_RANGE)
            {
                imu_buffer[ix + i] = get_sign(imu_buffer[ix + i]) * MAX_ACCEPTED_RANGE;
            }

            // Converte de g para m/s2
            imu_buffer[ix + i] *= CONVERT_G_TO_MS2;
        }

        // Aguarda ate a proxima amostra
        uint64_t now = micros();
        if (next_tick > now)
        {
            delayMicroseconds(next_tick - now);
        }
    }

    // Cria o sinal usado pelo Edge Impulse
    signal_t signal;
    int error = numpy::signal_from_buffer(imu_buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

    // Verifica erro ao criar sinal
    if (error != 0)
    {
        Serial.print("Signal error: ");
        Serial.println(error);
        return;
    }

    // Guarda o resultado da classificacao
    ei_impulse_result_t result = {0};

    // Executa o modelo
    error = run_classifier(&signal, &result, false);

    // Verifica erro ao rodar o modelo
    if (error != EI_IMPULSE_OK)
    {
        Serial.print("Classifier error: ");
        Serial.println(error);
        return;
    }

    // Mostra todos os labels e o melhor resultado
    print_classification(result);
    Serial.println();
}

void setup()
{
    Serial.begin(9600);

    while (!Serial);

    Serial.println("Welcome to the IMU test for the built-in IMU on the Nano 33 BLE Sense");
    Serial.println();
    Serial.println("Available commands:");
    Serial.println("a - display accelerometer readings in g's in x, y, and z directions");
    Serial.println("g - display gyroscope readings in deg/s in x, y, and z directions");
    Serial.println("m - display magnetometer readings in uT in x, y, and z directions");
    Serial.println("i - run movement inference using accelerometer");
    Serial.println();

    if (!IMU.begin())
    {
        Serial.println("Failed to initialize IMU!");
        while (1);
    }

    Serial.println("IMU initialized successfully.");
    Serial.println("Send a, g, m, or i to choose the output.");
    Serial.println();

    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 3)
    {
        Serial.println("Model must use 3 accelerometer axes.");
        while (1);
    }
}

void loop()
{
    // Verifica comandos do Serial Monitor
    if (Serial.available() > 0)
    {
        char command = Serial.read();

        if (command == 'a' || command == 'g' || command == 'm' || command == 'i')
        {
            mode = command;

            Serial.print("Selected mode: ");

            if (mode == 'a')
            {
                Serial.println("Accelerometer");
            }
            else if (mode == 'g')
            {
                Serial.println("Gyroscope");
            }
            else if (mode == 'm')
            {
                Serial.println("Magnetometer");
            }
            else if (mode == 'i')
            {
                Serial.println("Inference");
            }

            delay(500);
        }
    }

    float x, y, z;

    if (mode == 'a')
    {
        if (IMU.accelerationAvailable())
        {
            IMU.readAcceleration(x, y, z);

            // Formato compativel com Serial Plotter
            Serial.print("Ax:");
            Serial.print(x);
            Serial.print("\tAy:");
            Serial.print(y);
            Serial.print("\tAz:");
            Serial.println(z);
        }
    }
    else if (mode == 'g')
    {
        if (IMU.gyroscopeAvailable())
        {
            IMU.readGyroscope(x, y, z);

            Serial.print("Gx:");
            Serial.print(x);
            Serial.print("\tGy:");
            Serial.print(y);
            Serial.print("\tGz:");
            Serial.println(z);
        }
    }
    else if (mode == 'm')
    {
        if (IMU.magneticFieldAvailable())
        {
            IMU.readMagneticField(x, y, z);

            Serial.print("Mx:");
            Serial.print(x);
            Serial.print("\tMy:");
            Serial.print(y);
            Serial.print("\tMz:");
            Serial.println(z);
        }
    }
    else if (mode == 'i')
    {
        run_inference();
    }

    delay(100);
}
