"""Read the CI1306 AI UART on macOS (921600 8N1)."""
import argparse
import array
from datetime import datetime
import fcntl
import os
import termios
import tty


NAMES = {
    0x03: "ACK", 0x04: "ERROR", 0x05: "PING", 0x06: "PONG",
    0x10: "WAKE_DETECTED", 0x11: "DING_DONE", 0x12: "UPLINK_READY",
    0x16: "STATE_REPORT", 0x17: "LOCAL_CMD", 0x18: "FIRMWARE_INFO",
    0x22: "START_DOWNLINK", 0x23: "STOP_DOWNLINK", 0x24: "SET_VOLUME",
    0x25: "ENTER_WAKEUP_WAIT", 0x26: "FINISH_DOWNLINK", 0x27: "DOWNLINK_ABORTED",
    0x28: "ENTER_OTA_MODE", 0x29: "SET_CONVERSATION_STATE",
}
STATES = {0: "booting", 1: "waiting_wakeup", 2: "dialogue_listening",
          4: "downlink_playing", 6: "error"}


def crc8(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ (0x07 if crc & 0x80 else 0)) & 0xff
    return crc


def describe(kind, data):
    if kind == 0x10 and len(data) >= 2:
        return f"wake_id={int.from_bytes(data[:2], 'little')}"
    if kind == 0x12 and len(data) >= 4:
        return f"sample_rate={int.from_bytes(data[:2], 'little')} bits={data[2]} channels={data[3]}"
    if kind == 0x16 and data:
        return f"state={data[0]}/{STATES.get(data[0], 'unknown')}"
    if kind == 0x03 and len(data) >= 2:
        return f"ack_seq={data[0]} status={data[1]}"
    if kind in (0x05, 0x06) and len(data) >= 2:
        return f"role={data[0]} proto={data[1]}"
    return ""


def monitor(fd, raw=False):
    pending = bytearray()
    line = bytearray()
    while chunk := os.read(fd, 4096):
        if raw:
            print("[RX]", chunk.hex(" ").upper(), flush=True)
            continue
        pending.extend(chunk)
        while pending:
            if pending[0] != 0xA5:
                byte = pending.pop(0)
                if byte not in (10, 13):
                    line.append(byte)
                if line and (byte in (10, 13) or len(line) >= 4096):
                    try:
                        message = line.decode("utf-8")
                    except UnicodeDecodeError:
                        message = ""
                    if message and message.expandtabs().isprintable():
                        print("[TEXT]", message, flush=True)
                    else:
                        print("[RX]", line.hex(" ").upper(), flush=True)
                    line.clear()
                continue
            if len(pending) < 2:
                break
            if pending[1] != 0x5A:
                del pending[0]
                continue
            if len(pending) < 7:
                break
            length = int.from_bytes(pending[5:7], "little")
            if pending[2] != 4 or length > 64:
                del pending[0]
                continue
            size = 8 + length
            if len(pending) < size:
                break
            frame = pending[:size]
            stamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            kind = frame[3]
            expected = crc8(frame[2:-1])
            if expected == frame[-1]:
                name = NAMES.get(kind, f"TYPE_0x{kind:02X}")
                detail = describe(kind, frame[7:-1])
                message = f"{name} seq={frame[4]} len={length} {detail}".rstrip()
                del pending[:size]
            else:
                message = f"BAD_CRC got={frame[-1]:02X} calc={expected:02X}"
                next_header = pending.find(b"\xa5\x5a", 1, size + 1)
                del pending[:next_header if next_header >= 0 else size - int(frame[-1] == 0xA5)]
            print(f"[{stamp}] {message} | {frame.hex(' ').upper()}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="CI UART adapter, e.g. /dev/cu.usbserial-120")
    parser.add_argument("--raw", action="store_true", help="show every received byte as hex")
    parser.add_argument("--baud", type=int, default=921600)
    args = parser.parse_args()
    if args.baud <= 0:
        parser.error("--baud must be positive")
    fd = os.open(args.port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        original = termios.tcgetattr(fd)
        try:
            tty.setraw(fd, termios.TCSANOW)
            attrs = termios.tcgetattr(fd)
            attrs[2] &= ~(termios.CSTOPB | termios.CRTSCTS | termios.HUPCL)
            attrs[2] |= termios.CLOCAL | termios.CREAD
            termios.tcsetattr(fd, termios.TCSANOW, attrs)
            # Darwin IOSSIOSPEED sets nonstandard baud rates on USB serial adapters.
            speed = array.array("L", [args.baud])  # Darwin speed_t is unsigned long.
            fcntl.ioctl(fd, 0x80005402 | (speed.itemsize << 16), speed)
            os.set_blocking(fd, True)
            print(f"{args.port}  {args.baud} 8N1  (Ctrl+C to stop)", flush=True)
            monitor(fd, args.raw)
        finally:
            termios.tcsetattr(fd, termios.TCSANOW, original)
    finally:
        os.close(fd)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
    except (OSError, termios.error) as exc:
        raise SystemExit(f"CI UART: {exc}")
