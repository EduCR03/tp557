import argparse
import time
from collections import deque

import matplotlib.pyplot as plt
import serial
from serial.tools import list_ports


BAUD_RATE = 115200
WINDOW_SIZE = 300


def list_serial_ports():
    ports = list(list_ports.comports())

    print("Portas encontradas:")
    for port in ports:
        print(f"  {port.device} - {port.description}")


def read_startup_messages(serial_port, timeout=5):
    start_time = time.monotonic()

    print("Aguardando Arduino iniciar...")

    while time.monotonic() - start_time < timeout:
        line = serial_port.readline().decode(errors="ignore").strip()

        if line:
            print(line)


def read_audio_sample(serial_port):
    line = serial_port.readline().decode(errors="ignore").strip()

    if not line:
        return None

    try:
        return int(line)
    except ValueError:
        return None


def main():
    parser = argparse.ArgumentParser(description="Visualizador simples da onda do microfone.")
    parser.add_argument("--port", help="Porta serial. Exemplo: COM5")
    parser.add_argument("--baud", type=int, default=BAUD_RATE, help="Velocidade serial")
    parser.add_argument("--window", type=int, default=WINDOW_SIZE, help="Quantidade de amostras na tela")
    parser.add_argument("--step", type=int, default=4, help="Pula amostras para deixar mais leve")
    args = parser.parse_args()

    if not args.port:
        list_serial_ports()
        args.port = input("Digite a porta serial: ").strip()

    print("Abrindo porta serial...")
    serial_port = serial.Serial(args.port, args.baud, timeout=1)
    time.sleep(1)

    read_startup_messages(serial_port)
    serial_port.reset_input_buffer()

    print("Enviando comando click...")
    serial_port.write(b"click\n")
    serial_port.flush()

    samples = deque([0] * args.window, maxlen=args.window)

    plt.ion()
    fig, ax = plt.subplots()
    x_axis = list(range(args.window))
    line, = ax.plot(x_axis, list(samples))

    ax.set_title("Onda do microfone")
    ax.set_xlabel("Amostras")
    ax.set_ylabel("Amplitude")
    ax.set_ylim(-3000, 3000)
    ax.set_xlim(0, args.window - 1)
    ax.grid(True)

    print("Mostrando onda. Feche a janela para sair.")

    sample_count = 0

    while plt.fignum_exists(fig.number):
        # Le menos amostras por atualizacao para nao travar
        for _ in range(20):
            sample = read_audio_sample(serial_port)

            if sample is not None:
                sample_count += 1

                if sample_count % args.step == 0:
                    samples.append(sample)

        line.set_ydata(list(samples))
        fig.canvas.draw_idle()
        fig.canvas.flush_events()
        plt.pause(0.05)

    print("Parando gravacao...")
    serial_port.write(b"click\n")
    serial_port.close()


if __name__ == "__main__":
    main()
