import argparse
import time

import cv2
import numpy as np
import serial
from serial.tools import list_ports


WIDTH = 176
HEIGHT = 144
BYTES_PER_PIXEL = 2
FRAME_SIZE = WIDTH * HEIGHT * BYTES_PER_PIXEL
FRAME_START_MARKER = b"\xAA\x55\xAA\x55"
READY_TEXT = "Available commands:"


def list_serial_ports():
    ports = list(list_ports.comports())

    print("Portas encontradas:")
    for port in ports:
        print(f"  {port.device} - {port.description}")


def rgb565_to_rgb(frame_bytes):
    raw_bytes = np.frombuffer(frame_bytes, dtype=np.uint8)
    high_byte = raw_bytes[0::2].astype(np.uint16)
    low_byte = raw_bytes[1::2].astype(np.uint16)

    # Mesmo padrao do notebook da aula: pixel 16 bits em big endian
    pixels = (high_byte << 8) | low_byte

    # Converte RGB565 para RGB888
    red = ((pixels >> 11) & 0x1F) << 3
    green = ((pixels >> 5) & 0x3F) << 2
    blue = (pixels & 0x1F) << 3

    image = np.stack((red, green, blue), axis=1).astype(np.uint8)

    return image.reshape((HEIGHT, WIDTH, 3))


def rgb_to_bgr(image):
    # OpenCV mostra imagem em BGR
    return cv2.cvtColor(image, cv2.COLOR_RGB2BGR)


def draw_center_marker(frame):
    # Copia a imagem para criar marcador transparente
    overlay = frame.copy()

    height, width = frame.shape[:2]

    # Calcula centro da imagem
    center_x = width // 2
    center_y = height // 2

    # Define tamanho da area onde a mao deve ficar
    box_width = int(width * 0.38)
    box_height = int(height * 0.55)

    # Calcula cantos do retangulo
    x1 = center_x - box_width // 2
    y1 = center_y - box_height // 2
    x2 = center_x + box_width // 2
    y2 = center_y + box_height // 2

    # Desenha retangulo suave no centro
    cv2.rectangle(overlay, (x1, y1), (x2, y2), (80, 220, 80), 2)

    # Desenha uma cruz pequena no centro
    cv2.line(overlay, (center_x - 15, center_y), (center_x + 15, center_y), (80, 220, 80), 1)
    cv2.line(overlay, (center_x, center_y - 15), (center_x, center_y + 15), (80, 220, 80), 1)

    # Mistura marcador com a imagem
    cv2.addWeighted(overlay, 0.45, frame, 0.55, 0, frame)


def read_startup_messages(serial_port, timeout=10):
    start_time = time.monotonic()

    print("Aguardando Arduino iniciar...")

    while time.monotonic() - start_time < timeout:
        line = serial_port.readline().decode(errors="ignore").strip()

        if not line:
            continue

        print(line)

        if READY_TEXT in line:
            return True

    print("Menu nao encontrado, tentando enviar comando mesmo assim.")
    return False


def read_exactly(serial_port, size, timeout=5):
    start_time = time.monotonic()
    data = bytearray()

    # Continua lendo ate completar um frame
    while len(data) < size:
        chunk = serial_port.read(size - len(data))

        if chunk:
            data.extend(chunk)
            start_time = time.monotonic()
        elif time.monotonic() - start_time > timeout:
            return None

    return bytes(data)


def wait_for_frame_start(serial_port, timeout=12):
    start_time = time.monotonic()
    buffer = bytearray()
    line_buffer = bytearray()
    last_label = None

    # Procura o marcador enviado antes de cada imagem
    while True:
        byte = serial_port.read(1)

        if not byte:
            if time.monotonic() - start_time > timeout:
                return False
            continue

        buffer.extend(byte)

        if len(buffer) > len(FRAME_START_MARKER):
            buffer = buffer[-len(FRAME_START_MARKER):]

        if bytes(buffer) == FRAME_START_MARKER:
            return True, last_label

        value = byte[0]

        # Le mensagens de texto enviadas entre os frames
        if value in (10, 13):
            line = line_buffer.decode(errors="ignore").strip()
            line_buffer.clear()

            if line.startswith("Label:"):
                last_label = line.replace("Label:", "").strip()
        elif 32 <= value <= 126 and len(line_buffer) < 120:
            line_buffer.extend(byte)


def main():
    parser = argparse.ArgumentParser(description="Visualizador simples da camera OV7675.")
    parser.add_argument("--port", help="Porta serial. Exemplo: COM5")
    parser.add_argument("--baud", type=int, default=115200, help="Velocidade serial")
    parser.add_argument("--scale", type=int, default=3, help="Escala da janela")
    parser.add_argument("--save", help="Salva o ultimo frame em PNG")
    parser.add_argument("--infer", action="store_true", help="Mostra camera e label detectado")
    args = parser.parse_args()

    if not args.port:
        list_serial_ports()
        args.port = input("Digite a porta serial: ").strip()

    print("Abrindo porta serial...")
    serial_port = serial.Serial(args.port, args.baud, timeout=1)
    time.sleep(1)

    read_startup_messages(serial_port)
    serial_port.reset_input_buffer()

    if args.infer:
        print("Enviando comando live_infer...")
        serial_port.write(b"live_infer\n")
    else:
        print("Enviando comando live...")
        serial_port.write(b"live\n")

    serial_port.flush()

    print("Aguardando primeiro frame...")

    print("Mostrando imagem. Pressione q para sair.")
    last_frame = None
    label_text = ""
    use_marker = True

    while True:
        if use_marker:
            found_marker, new_label = wait_for_frame_start(serial_port)

            if new_label:
                label_text = new_label

            if not found_marker:
                print("Marcador nao encontrado. Tentando leitura bruta.")
                print("Se travar aqui, regrave o Arduino com o codigo atualizado.")
                use_marker = False

        frame_bytes = read_exactly(serial_port, FRAME_SIZE)

        if frame_bytes is None:
            print("Nenhum frame recebido.")
            print("Confira se a porta esta correta e se o Arduino foi gravado.")
            break

        rgb_frame = rgb565_to_rgb(frame_bytes)
        bgr_frame = rgb_to_bgr(rgb_frame)
        last_frame = bgr_frame

        # Aumenta a imagem para facilitar visualizacao
        frame = cv2.resize(
            bgr_frame,
            (WIDTH * args.scale, HEIGHT * args.scale),
            interpolation=cv2.INTER_NEAREST
        )

        draw_center_marker(frame)

        if label_text:
            cv2.rectangle(frame, (0, 0), (260, 34), (0, 0, 0), -1)
            cv2.putText(
                frame,
                f"Label: {label_text}",
                (10, 24),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.7,
                (255, 255, 255),
                2
            )

        cv2.imshow("TinyML Shield Camera", frame)

        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    serial_port.write(b"single\n")
    serial_port.close()
    cv2.destroyAllWindows()

    if args.save and last_frame is not None:
        cv2.imwrite(args.save, last_frame)
        print(f"Imagem salva em: {args.save}")


if __name__ == "__main__":
    main()
