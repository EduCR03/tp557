#include <Arduino.h>
#include <Arduino_OV767X.h>
#include <Projeto-Aula1_inferencing.h>

#define BUTTON_PIN 13

bool live_mode = false;
bool infer_mode = false;
bool capture_requested = false;

byte image[176 * 144 * 2];
const byte frame_start_marker[] = {0xAA, 0x55, 0xAA, 0x55};

uint16_t bytes_per_frame = 0;

void start_shield()
{
    pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void start_camera()
{
    while (!Camera.begin(QCIF, RGB565, 1))
    {
        Serial.println("Failed to initialize camera!");
        delay(1000);
    }

    bytes_per_frame = Camera.width() * Camera.height() * Camera.bytesPerPixel();

    Serial.println("Camera initialized successfully.");
    Serial.print("Width: ");
    Serial.println(Camera.width());
    Serial.print("Height: ");
    Serial.println(Camera.height());
    Serial.print("Bytes per pixel: ");
    Serial.println(Camera.bytesPerPixel());
    Serial.print("Bytes per frame: ");
    Serial.println(bytes_per_frame);
}

void print_menu()
{
    Serial.println();
    Serial.println("Welcome to the OV7675 camera test");
    Serial.println();
    Serial.println("Available commands:");
    Serial.println("single  - take a single image and print hexadecimal pixels");
    Serial.println("live    - stream raw camera bytes over serial");
    Serial.println("live_infer - stream camera and print detected label");
    Serial.println("capture - capture one image in single mode");
    Serial.println("infer   - run Edge Impulse inference");
    Serial.println("stop    - stop live or inference mode");
    Serial.println();
    Serial.println("In single mode, you can also press the TinyML Shield button.");
    Serial.println();
}

bool read_shield_button()
{
    bool reading = digitalRead(BUTTON_PIN);

    static bool last_reading = HIGH;
    static bool button_state = HIGH;
    static unsigned long last_debounce_time = 0;

    const unsigned long debounce_delay = 50;

    if (reading != last_reading)
    {
        last_debounce_time = millis();
    }

    if ((millis() - last_debounce_time) > debounce_delay)
    {
        if (reading != button_state)
        {
            button_state = reading;

            if (button_state == LOW)
            {
                last_reading = reading;
                return true;
            }
        }
    }

    last_reading = reading;
    return false;
}

int get_camera_data(size_t offset, size_t length, float *out_ptr)
{
    // Largura real da camera.
    int camera_width = Camera.width();

    // Altura real da camera.
    int camera_height = Camera.height();

    // Largura que o modelo espera.
    int model_width = EI_CLASSIFIER_INPUT_WIDTH;

    // Altura que o modelo espera.
    int model_height = EI_CLASSIFIER_INPUT_HEIGHT;

    // Posicao inicial do corte na imagem.
    int crop_x = 0;
    int crop_y = 0;

    // Tamanho inicial usando a imagem inteira.
    int crop_width = camera_width;
    int crop_height = camera_height;

    // Ajusta o corte para manter a proporcao do modelo.
    if ((camera_width * model_height) > (camera_height * model_width))
    {
        // Corta as laterais quando a camera e mais larga.
        crop_width = (camera_height * model_width) / model_height;

        // Centraliza o corte na horizontal.
        crop_x = (camera_width - crop_width) / 2;
    }
    else
    {
        // Corta em cima e embaixo quando a camera e mais alta.
        crop_height = (camera_width * model_height) / model_width;

        // Centraliza o corte na vertical.
        crop_y = (camera_height - crop_height) / 2;
    }

    // Percorre os pixels pedidos pelo Edge Impulse.
    for (size_t i = 0; i < length; i++)
    {
        // Indice do pixel dentro da entrada do modelo.
        size_t pixel = offset + i;

        // Posicao x do pixel no modelo.
        int model_x = pixel % model_width;

        // Posicao y do pixel no modelo.
        int model_y = pixel / model_width;

        // Converte a posicao do modelo para a imagem da camera.
        int camera_x = crop_x + ((model_x * crop_width) / model_width);
        int camera_y = crop_y + ((model_y * crop_height) / model_height);

        // Encontra o pixel dentro do vetor RGB565.
        size_t image_index = (camera_y * camera_width + camera_x) * 2;

        // Junta os dois bytes do pixel RGB565.
        uint16_t rgb565 = ((uint16_t)image[image_index] << 8) | image[image_index + 1];

        // Extrai o vermelho do RGB565.
        uint8_t red = ((rgb565 >> 11) & 0x1F) << 3;

        // Extrai o verde do RGB565.
        uint8_t green = ((rgb565 >> 5) & 0x3F) << 2;

        // Extrai o azul do RGB565.
        uint8_t blue = (rgb565 & 0x1F) << 3;

        // Converte RGB para cinza usando pesos padrao.
        uint8_t gray = (uint8_t)((red * 30 + green * 59 + blue * 11) / 100);

        // Repete o cinza nos tres canais.
        uint32_t gray_rgb = ((uint32_t)gray << 16) | ((uint32_t)gray << 8) | gray;

        // Entrega o pixel cinza no formato que o Edge Impulse espera.
        out_ptr[i] = (float)gray_rgb;
    }

    // Retorna zero quando tudo deu certo.
    return 0;
}

void run_inference(bool capture_new_image)
{
    // Guarda o tempo da ultima inferencia.
    static unsigned long last_inference_time = 0;

    // Verifica se ainda nao passou 1 segundo.
    if ((millis() - last_inference_time) < 1000)
    {
        // Sai da funcao para nao rodar toda hora.
        return;
    }

    // Atualiza o tempo da ultima inferencia.
    last_inference_time = millis();

    // Captura uma imagem nova quando necessario.
    if (capture_new_image)
    {
        Camera.readFrame(image);
    }

    // Cria o sinal usado pelo Edge Impulse.
    ei::signal_t signal;

    // Define a quantidade de pixels da entrada do modelo.
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;

    // Define a funcao que entrega os pixels para o modelo.
    signal.get_data = &get_camera_data;

    // Cria a variavel que recebe o resultado do modelo.
    ei_impulse_result_t result = {0};

    // Executa a classificacao da imagem.
    EI_IMPULSE_ERROR response = run_classifier(&signal, &result, false);

    // Verifica se ocorreu erro na classificacao.
    if (response != EI_IMPULSE_OK)
    {
        // Mostra o codigo do erro.
        Serial.print("Erro: ");

        // Imprime o valor retornado pelo Edge Impulse.
        Serial.println(response);

        // Sai da funcao se deu erro.
        return;
    }

    // Comeca assumindo que o primeiro label venceu.
    int best_index = 0;

    // Guarda a pontuacao do primeiro label.
    float best_score = result.classification[0].value;

    // Percorre os outros labels do modelo.
    for (int i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++)
    {
        // Compara a pontuacao atual com a melhor.
        if (result.classification[i].value > best_score)
        {
            // Guarda o indice do melhor label.
            best_index = i;

            // Guarda a melhor pontuacao encontrada.
            best_score = result.classification[i].value;
        }
    }

    // Escreve o texto fixo no terminal.
    Serial.print("Label: ");

    // Mostra o label com maior pontuacao.
    Serial.println(ei_classifier_inferencing_categories[best_index]);
}

void detect_command_serial()
{
    if (Serial.available())
    {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command == "single")
        {
            live_mode = false;
            infer_mode = false;
            Serial.println();
            Serial.println("Camera in single mode.");
            Serial.println("Type 'capture' or press the shield button to capture an image.");
        }
        else if (command == "capture")
        {
            if (!live_mode && !infer_mode)
            {
                capture_requested = true;
            }
            else
            {
                Serial.println();
                Serial.println("Camera is in live or inference mode. Type 'single' first.");
            }
        }
        else if (command == "live")
        {
            infer_mode = false;
            Serial.println();
            Serial.println("Raw image data will begin streaming in 5 seconds...");
            Serial.println("Warning: raw bytes will not look readable in Serial Monitor.");
            delay(5000);
            live_mode = true;
        }
        else if (command == "live_infer")
        {
            Serial.println();
            Serial.println("Raw image data and labels will begin streaming in 5 seconds...");
            Serial.println("Warning: raw bytes will not look readable in Serial Monitor.");
            delay(5000);
            live_mode = true;
            infer_mode = true;
        }
        else if (command == "infer")
        {
            live_mode = false;
            infer_mode = true;
            Serial.println();
            Serial.println("Inference mode.");
        }
        else if (command == "stop")
        {
            live_mode = false;
            infer_mode = false;
            Serial.println();
            Serial.println("Stopped.");
        }
        else if (command.length() > 0)
        {
            Serial.println();
            Serial.println("Unknown command.");
            print_menu();
        }
    }
}

void capture_image()
{
    if (capture_requested)
    {
        capture_requested = false;

        Serial.println();
        Serial.println("Capturing image...");
        Camera.readFrame(image);

        Serial.println("Image data will be printed out in 3 seconds...");
        delay(3000);

        for (int i = 0; i < bytes_per_frame - 1; i += 2)
        {
            uint16_t pixel = ((uint16_t)image[i + 1] << 8) | image[i];

            Serial.print("0x");

            if (pixel < 0x1000)
                Serial.print("0");
            if (pixel < 0x0100)
                Serial.print("0");
            if (pixel < 0x0010)
                Serial.print("0");

            Serial.print(pixel, HEX);

            if (i != bytes_per_frame - 2)
            {
                Serial.print(", ");
            }
        }

        Serial.println();
        Serial.println("Capture complete.");
    }
}

void setup()
{
    Serial.begin(115200);

    start_shield();
    start_camera();

    print_menu();
}

void loop()
{
    if (read_shield_button())
    {
        if (!live_mode && !infer_mode)
        {
            capture_requested = true;
        }
    }

    detect_command_serial();

    if (live_mode)
    {
        Camera.readFrame(image);
        Serial.write(frame_start_marker, sizeof(frame_start_marker));
        Serial.write(image, bytes_per_frame);

        if (infer_mode)
        {
            run_inference(false);
        }
    }
    else if (infer_mode)
    {
        run_inference(true);
    }

    capture_image();
}
