import argparse
import time
from collections import deque

import matplotlib.pyplot as plt
import serial
from serial.tools import list_ports


BAUD_RATE = 9600
WINDOW_SIZE = 120


def list_serial_ports():
    ports = list(list_ports.comports())

    print("Portas encontradas:")
    for port in ports:
        print(f"  {port.device} - {port.description}")


def read_startup_messages(serial_port, timeout=3):
    start_time = time.monotonic()

    print("Aguardando Arduino iniciar...")

    while time.monotonic() - start_time < timeout:
        line = serial_port.readline().decode(errors="ignore").strip()

        if line:
            print(line)


def get_mode_info(mode):
    if mode == "a":
        labels = ("Ax", "Ay", "Az")
        title = "Acelerometro"
        limit = 2
    elif mode == "g":
        labels = ("Gx", "Gy", "Gz")
        title = "Giroscopio"
        limit = 300
    else:
        labels = ("Mx", "My", "Mz")
        title = "Magnetometro"
        limit = 100

    return labels, title, limit


def read_imu_values(serial_port, labels):
    line = serial_port.readline().decode(errors="ignore").strip()

    if not line:
        return None

    values = {}
    parts = line.split("\t")

    # Separa cada eixo enviado pelo Arduino
    for part in parts:
        if ":" not in part:
            continue

        name, value = part.split(":", 1)
        name = name.strip()
        value = value.strip()

        if name in labels:
            try:
                values[name] = float(value)
            except ValueError:
                return None

    if len(values) != 3:
        return None

    return values[labels[0]], values[labels[1]], values[labels[2]]


def main():
    parser = argparse.ArgumentParser(description="Visualizador simples da IMU.")
    parser.add_argument("--port", help="Porta serial. Exemplo: COM5")
    parser.add_argument("--baud", type=int, default=BAUD_RATE, help="Velocidade serial")
    parser.add_argument("--mode", choices=["a", "g", "m"], default="a", help="a, g ou m")
    parser.add_argument("--window", type=int, default=WINDOW_SIZE, help="Quantidade de pontos na tela")
    parser.add_argument("--limit", type=float, help="Limite vertical do grafico")
    args = parser.parse_args()

    if not args.port:
        list_serial_ports()
        args.port = input("Digite a porta serial: ").strip()

    labels, title, default_limit = get_mode_info(args.mode)

    if args.limit is None:
        args.limit = default_limit

    print("Abrindo porta serial...")
    serial_port = serial.Serial(args.port, args.baud, timeout=1)
    time.sleep(1)

    read_startup_messages(serial_port)
    serial_port.reset_input_buffer()

    print(f"Enviando modo {args.mode}...")
    serial_port.write((args.mode + "\n").encode())
    serial_port.flush()
    time.sleep(0.5)
    serial_port.reset_input_buffer()

    x_values = deque([0] * args.window, maxlen=args.window)
    y_values = deque([0] * args.window, maxlen=args.window)
    z_values = deque([0] * args.window, maxlen=args.window)

    plt.ion()
    fig, ax = plt.subplots()
    x_axis = list(range(args.window))

    # Cria uma linha para cada eixo
    line_x, = ax.plot(x_axis, list(x_values), label=labels[0])
    line_y, = ax.plot(x_axis, list(y_values), label=labels[1])
    line_z, = ax.plot(x_axis, list(z_values), label=labels[2])

    ax.set_title(f"IMU - {title}")
    ax.set_xlabel("Amostras")
    ax.set_ylabel("Valor")
    ax.set_ylim(-args.limit, args.limit)
    ax.set_xlim(0, args.window - 1)
    ax.grid(True)
    ax.legend()

    print("Mostrando IMU. Feche a janela para sair.")

    while plt.fignum_exists(fig.number):
        # Le alguns pontos por atualizacao
        for _ in range(5):
            values = read_imu_values(serial_port, labels)

            if values is not None:
                x, y, z = values
                x_values.append(x)
                y_values.append(y)
                z_values.append(z)

        line_x.set_ydata(list(x_values))
        line_y.set_ydata(list(y_values))
        line_z.set_ydata(list(z_values))

        fig.canvas.draw_idle()
        fig.canvas.flush_events()
        plt.pause(0.05)

    serial_port.close()


if __name__ == "__main__":
    main()
