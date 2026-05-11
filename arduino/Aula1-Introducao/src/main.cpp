#include <Arduino.h>
#include <TinyMLShield.h>

bool live_mode = false;
bool capture_requested = false;

byte image[176 * 144 * 2];

uint16_t bytes_per_frame = 0;

void start_shield()
{
    pinMode(BUTTON_PIN, OUTPUT);
    digitalWrite(BUTTON_PIN, HIGH);
    nrf_gpio_cfg_out_with_input(digitalPinToPinName(BUTTON_PIN));
}

void start_camera()
{
    while (!Camera.begin(QCIF, RGB565, 1, OV7675))
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
    Serial.println("capture - capture one image in single mode");
    Serial.println();
    Serial.println("In single mode, you can also press the TinyML Shield button.");
    Serial.println();
}

bool read_shield_button()
{
    bool reading = nrf_gpio_pin_read(digitalPinToPinName(BUTTON_PIN));

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

void detect_command_serial()
{
    if (Serial.available())
    {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command == "single")
        {
            live_mode = false;
            Serial.println();
            Serial.println("Camera in single mode.");
            Serial.println("Type 'capture' or press the shield button to capture an image.");
        }
        else if (command == "capture")
        {
            if (!live_mode)
            {
                capture_requested = true;
            }
            else
            {
                Serial.println();
                Serial.println("Camera is in live mode. Type 'single' first.");
            }
        }
        else if (command == "live")
        {
            Serial.println();
            Serial.println("Raw image data will begin streaming in 5 seconds...");
            Serial.println("Warning: raw bytes will not look readable in Serial Monitor.");
            delay(5000);
            live_mode = true;
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
        if (!live_mode)
        {
            capture_requested = true;
        }
    }

    detect_command_serial();

    if (live_mode)
    {
        Camera.readFrame(image);
        Serial.write(image, bytes_per_frame);
    }

    capture_image();
}
