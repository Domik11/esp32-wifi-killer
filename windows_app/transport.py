"""Порт принадлежит одному рабочему потоку; Tk не блокируется чтением USB."""
import queue
import threading
import time
import serial


class SerialWorker:
    def __init__(self, port, events):
        self.port = port
        self.events = events
        self.commands = queue.Queue()
        self.stopping = threading.Event()
        self.thread = threading.Thread(target=self.run, daemon=True)

    def start(self):
        self.thread.start()

    def send(self, line):
        if '\n' in line or '\r' in line or len(line.encode('utf-8')) > 256:
            raise ValueError('Недопустимая строка USB-протокола.')
        self.commands.put(('send', line))

    def reset(self):
        self.commands.put(('reset', None))

    def stop(self):
        self.stopping.set()

    def run(self):
        connection = None
        try:
            connection = serial.Serial(port=None, baudrate=115200, timeout=0.1, write_timeout=2)
            connection.dtr = False
            connection.rts = False
            connection.port = self.port
            connection.open()
            self.events.put(('port_open', self.port))
            self.reset()
            pending = bytearray()
            while not self.stopping.is_set():
                try:
                    command, payload = self.commands.get_nowait()
                    if command == 'reset':
                        pending.clear()
                        connection.reset_input_buffer()
                        connection.dtr = False
                        connection.rts = True
                        time.sleep(0.15)
                        connection.rts = False
                    elif command == 'send':
                        connection.write((payload + '\n').encode('utf-8'))
                        connection.flush()
                except queue.Empty:
                    pass
                data = connection.read(max(1, min(connection.in_waiting, 4096)))
                pending.extend(data)
                while b'\n' in pending:
                    line, _, rest = pending.partition(b'\n')
                    pending = bytearray(rest)
                    self.events.put(('line', line.decode('utf-8', errors='replace').rstrip('\r')))
                if len(pending) > 8192:
                    pending.clear()
        except (serial.SerialException, OSError, ValueError) as error:
            self.events.put(('port_error', str(error)))
        finally:
            if connection:
                connection.close()
            self.events.put(('port_closed', None))
