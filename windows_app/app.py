"""Небольшой Windows-мастер ESP32. Настройки и пароль на диск не сохраняются."""
import queue
import time
import tkinter as tk
from tkinter import ttk, messagebox
from serial.tools import list_ports
from protocol import Session, decode_event
from transport import SerialWorker


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title('ESP32 Network — управление')
        self.geometry('1040x820')
        self.minsize(860, 800)
        self.configure(bg='#eff3f8')
        self.session = Session()
        self.worker = None
        self.events = queue.Queue()
        self.port = tk.StringVar()
        self.password = tk.StringVar()
        self.custom_name = tk.StringVar()
        self.hint = tk.StringVar(value='Подключите ESP32 по USB и откройте её COM-порт.')
        self.network_status = tk.StringVar(value='Wi-Fi: —')
        self.saved_status = tk.StringVar(value='Устройство: —')
        self.internet_status = tk.StringVar(value='Интернет: —')
        self.port_status = tk.StringVar(value='USB отключён')
        self.wait_started = 0
        style = ttk.Style(self)
        style.theme_use('clam')
        style.configure('.', font=('Segoe UI', 10), background='#eff3f8', foreground='#20304a')
        style.configure('TButton', padding=(12, 7))
        style.configure('Accent.TButton', background='#1668d4', foreground='white')
        style.map('Accent.TButton', background=[('disabled', '#b9cce7'), ('active', '#0c53ad')])
        style.configure('Treeview', rowheight=27, background='white', fieldbackground='white')
        style.configure('Treeview.Heading', font=('Segoe UI', 10, 'bold'), padding=5)
        body = ttk.Frame(self, padding=20)
        body.pack(fill='both', expand=True)
        ttk.Label(body, text='ESP32 Network', font=('Segoe UI', 23, 'bold')).pack(anchor='w')
        ttk.Label(body, text='Подключение к сети и поиск устройства через USB', foreground='#64748b').pack(anchor='w', pady=(0, 12))
        usb = ttk.LabelFrame(body, text='1   Плата', padding=10)
        usb.pack(fill='x', pady=(0, 10))
        self.port_box = ttk.Combobox(usb, textvariable=self.port, state='readonly', width=35)
        self.port_box.pack(side='left', padx=(0, 8))
        ttk.Button(usb, text='Обновить порты', command=self.refresh_ports).pack(side='left')
        self.open_button = ttk.Button(usb, text='Подключить USB', style='Accent.TButton', command=self.toggle_port)
        self.open_button.pack(side='left', padx=8)
        self.reset_button = ttk.Button(usb, text='Начать заново', command=self.reset_board, state='disabled')
        self.reset_button.pack(side='left')
        ttk.Label(usb, textvariable=self.port_status).pack(side='right', padx=8)
        self.banner = ttk.Label(body, textvariable=self.hint, wraplength=950, font=('Segoe UI', 11, 'bold'), foreground='#1668d4')
        self.banner.pack(anchor='w', pady=(0, 8))

        wifi = ttk.LabelFrame(body, text='2   Wi-Fi сеть', padding=10)
        wifi.pack(fill='x', pady=(0, 10))
        self.wifi_table = self.table(wifi, ('ssid', 'signal', 'security'), ('Название сети', 'Сигнал', 'Защита'), (550, 100, 140), 3)
        row = ttk.Frame(wifi)
        row.pack(fill='x', pady=(8, 0))
        ttk.Label(row, text='Пароль').pack(side='left')
        self.password_entry = ttk.Entry(row, textvariable=self.password, show='●', width=32)
        self.password_entry.pack(side='left', padx=8)
        self.show_password = tk.BooleanVar()
        ttk.Checkbutton(row, text='Показать', variable=self.show_password,
                        command=lambda: self.password_entry.configure(show='' if self.show_password.get() else '●')).pack(side='left')
        self.wifi_button = ttk.Button(row, text='Подключиться к Wi-Fi', command=self.choose_wifi, style='Accent.TButton', state='disabled')
        self.wifi_button.pack(side='right')

        devices = ttk.LabelFrame(body, text='3   Устройство в локальной сети', padding=10)
        devices.pack(fill='x', pady=(0, 10))
        self.device_table = self.table(devices, ('ip', 'mac', 'name'), ('IP-адрес', 'MAC-адрес', 'Имя'), (180, 220, 390), 3)
        row = ttk.Frame(devices)
        row.pack(fill='x', pady=(8, 0))
        ttk.Label(row, text='Своё имя (необязательно)').pack(side='left')
        ttk.Entry(row, textvariable=self.custom_name, width=28).pack(side='left', padx=8)
        self.device_button = ttk.Button(row, text='Выбрать и сохранить', command=self.choose_device, style='Accent.TButton', state='disabled')
        self.device_button.pack(side='right')
        state_row = ttk.Frame(body)
        state_row.pack(fill='x', pady=(0, 8))
        for var in (self.network_status, self.internet_status, self.saved_status):
            ttk.Label(state_row, textvariable=var, wraplength=330).pack(side='left', anchor='n', padx=(0, 14))

        log_frame = ttk.LabelFrame(body, text='Журнал платы', padding=8)
        log_frame.pack(fill='both', expand=True)
        self.log = tk.Text(log_frame, height=7, font=('Consolas', 9), bg='#132038', fg='#d5e4ff', relief='flat', state='disabled', wrap='word')
        self.log.pack(fill='both', expand=True, side='left')
        scroll = ttk.Scrollbar(log_frame, command=self.log.yview)
        scroll.pack(side='right', fill='y')
        self.log.configure(yscrollcommand=scroll.set)
        ttk.Label(body, text='Пароль передаётся только плате по USB и не сохраняется приложением. Перед прошивкой отключите USB-порт здесь.',
                  foreground='#64748b', wraplength=950).pack(anchor='w', pady=(8, 0))
        self.protocol('WM_DELETE_WINDOW', self.close)
        self.refresh_ports()
        self.after(80, self.poll)

    def table(self, parent, columns, headings, widths, height):
        frame = ttk.Frame(parent)
        frame.pack(fill='x')
        tree = ttk.Treeview(frame, columns=columns, show='headings', height=height, selectmode='browse')
        for col, heading, width in zip(columns, headings, widths):
            tree.heading(col, text=heading)
            tree.column(col, width=width, minwidth=70)
        tree.pack(fill='x', side='left', expand=True)
        scroll = ttk.Scrollbar(frame, command=tree.yview)
        scroll.pack(side='right', fill='y')
        tree.configure(yscrollcommand=scroll.set)
        return tree

    def refresh_ports(self):
        ports = sorted(list_ports.comports(), key=lambda p: (p.vid is None, p.device))
        self.port_map = {f'{p.device} — {p.description}': p.device for p in ports}
        self.port_box['values'] = list(self.port_map)
        if self.port.get() not in self.port_map:
            preferred = next((label for label in self.port_map if 'CH340' in label or 'CP210' in label), None)
            self.port.set(preferred or next(iter(self.port_map), ''))

    def append_log(self, text):
        self.log.configure(state='normal')
        self.log.insert('end', text + '\n')
        if int(self.log.index('end-1c').split('.')[0]) > 1200:
            self.log.delete('1.0', '250.0')
        self.log.see('end')
        self.log.configure(state='disabled')

    def toggle_port(self):
        if self.worker:
            self.worker.stop()
            self.open_button.configure(state='disabled')
            self.hint.set('Закрываю USB-порт…')
            return
        port = self.port_map.get(self.port.get())
        if not port:
            messagebox.showwarning('USB', 'Выберите COM-порт ESP32.')
            return
        self.events = queue.Queue()
        self.session = Session()
        self.worker = SerialWorker(port, self.events)
        self.open_button.configure(state='disabled')
        self.port_box.configure(state='disabled')
        self.hint.set('Открываю порт и перезапускаю ESP32…')
        self.worker.start()

    def reset_board(self):
        if self.worker:
            self.session = Session(stage='boot')
            self.password.set('')
            self.custom_name.set('')
            self.wifi_table.delete(*self.wifi_table.get_children())
            self.device_table.delete(*self.device_table.get_children())
            self.network_status.set('Wi-Fi: —')
            self.internet_status.set('Интернет: —')
            self.saved_status.set('Устройство: —')
            self.worker.reset()
            self.wait_started = time.monotonic()
            self.hint.set('Перезапуск ESP32. Ожидаю список Wi-Fi…')
            self.sync_buttons()

    def choose_wifi(self):
        try:
            if self.session.stage == 'wifi_password':
                from protocol import valid_password
                if not valid_password(self.password.get()):
                    raise ValueError('Введите пароль: 8–63 байта или 64 hex-символа.')
                self.worker.send(self.password.get())
                self.session.stage = 'connecting'
            else:
                selection = self.wifi_table.selection()
                if not selection:
                    raise ValueError('Выберите строку Wi-Fi сети в таблице.')
                self.worker.send(self.session.choose_wifi(int(selection[0]), self.password.get()))
            self.password.set('')
            self.hint.set('Подключение к Wi-Fi…')
            self.sync_buttons()
        except ValueError as error:
            messagebox.showwarning('Wi-Fi', str(error))

    def choose_device(self):
        try:
            selection = self.device_table.selection()
            if not selection:
                raise ValueError('Выберите устройство в таблице.')
            self.worker.send(self.session.choose_device(int(selection[0]), self.custom_name.get()))
            self.hint.set('Сохраняю выбранное устройство…')
            self.sync_buttons()
        except ValueError as error:
            messagebox.showwarning('Устройство', str(error))

    def sync_buttons(self):
        online = bool(self.worker)
        self.wifi_button.configure(state='normal' if online and self.session.stage in ('wifi_choice', 'wifi_password') else 'disabled')
        self.device_button.configure(state='normal' if online and self.session.stage == 'device_choice' else 'disabled')

    def handle_event(self, event):
        reply = self.session.accept(event)
        kind = event['event']
        if kind == 'hello':
            self.hint.set('Плата готова. Поиск Wi-Fi сетей…')
            self.password.set('')
        elif kind == 'wifi_scan':
            self.wifi_table.delete(*self.wifi_table.get_children())
            self.device_table.delete(*self.device_table.get_children())
            self.hint.set('Поиск Wi-Fi сетей…')
        elif kind == 'wifi' and isinstance(event.get('index'), int):
            key = str(event['index'])
            values = (event.get('ssid', ''), f"{event.get('rssi', '?')} dBm", 'Открытая' if event.get('open') else 'Пароль')
            if self.wifi_table.exists(key): self.wifi_table.item(key, values=values)
            else: self.wifi_table.insert('', 'end', iid=key, values=values)
        elif kind == 'request':
            hints = {'wifi_choice': 'Выберите Wi-Fi сеть, введите пароль и нажмите «Подключиться к Wi-Fi».',
                     'wifi_password': 'Передаю пароль плате…' if reply is not None else 'Введите пароль сети и нажмите «Подключиться к Wi-Fi».',
                     'device_choice': 'Выберите устройство в таблице. При желании укажите своё имя.',
                     'device_name': 'Сохраняю имя устройства…'}
            self.hint.set(hints.get(event.get('field'), 'Ожидание платы…'))
        elif kind == 'wifi_connecting':
            self.hint.set('Подключение к ' + event.get('ssid', '') + '…')
        elif kind == 'wifi_connected':
            self.network_status.set('Wi-Fi: ' + event.get('ssid', '') + ' · ' + event.get('ip', ''))
            self.hint.set('Wi-Fi подключён. Сканирование устройств может занять до минуты…')
        elif kind == 'device_scan':
            self.device_table.delete(*self.device_table.get_children())
            self.hint.set('Сканирование локальной сети и определение имён…')
        elif kind == 'device' and isinstance(event.get('index'), int):
            key = str(event['index'])
            values = tuple(event.get(k, '') for k in ('ip', 'mac', 'name'))
            if self.device_table.exists(key): self.device_table.item(key, values=values)
            else: self.device_table.insert('', 'end', iid=key, values=values)
        elif kind == 'saved':
            self.saved_status.set('Устройство: ' + event.get('name', '') + ' · ' + event.get('ip', ''))
            self.append_log('Сохранено: ' + event.get('mac', '') + ' / ' + event.get('ip', ''))
        elif kind == 'ready':
            self.hint.set('Настройка завершена. Работают поиск устройства и диагностика.')
        elif kind == 'status':
            if event.get('connected'):
                self.network_status.set('Wi-Fi подключён · IP ESP32: ' + event.get('ip', ''))
                self.internet_status.set('yandex.ru: ' + (f"{event.get('ping', '?')} мс" if event.get('internet') else 'нет ICMP-ответа'))
            else:
                self.network_status.set('Wi-Fi отключён · переподключение')
                self.internet_status.set('Интернет: —')
        elif kind == 'tracked':
            self.saved_status.set('Устройство: ' + ('найдено · ' + event.get('ip', '') if event.get('found') else 'поиск продолжается'))
        elif kind == 'error':
            self.hint.set(event.get('message', 'Ошибка платы.'))
            self.append_log(event.get('message', 'Ошибка платы.'))
        if reply is not None and self.worker:
            self.worker.send(reply)
        self.sync_buttons()

    def poll(self):
        try:
            for _ in range(300):
                kind, payload = self.events.get_nowait()
                if kind == 'port_open':
                    self.port_status.set(payload + ' · 115200')
                    self.open_button.configure(text='Отключить USB', state='normal')
                    self.reset_button.configure(state='normal')
                    self.wait_started = time.monotonic()
                elif kind == 'port_error':
                    self.hint.set('Не удалось работать с портом. Закройте Serial Monitor в VS Code / Arduino IDE.')
                    self.append_log('Ошибка USB: ' + payload)
                elif kind == 'port_closed':
                    self.worker = None
                    self.session = Session()
                    self.password.set('')
                    self.port_status.set('USB отключён')
                    self.open_button.configure(text='Подключить USB', state='normal')
                    self.port_box.configure(state='readonly')
                    self.reset_button.configure(state='disabled')
                    self.sync_buttons()
                elif kind == 'line':
                    event = decode_event(payload)
                    if event:
                        self.handle_event(event)
                    elif payload.strip():
                        self.append_log(payload)
            # Остаток очереди будет обработан на следующем такте.
        except queue.Empty:
            pass
        if self.worker and self.session.stage in ('offline', 'boot') and self.wait_started and time.monotonic() - self.wait_started > 12:
            self.hint.set('Нет ответа новой прошивки. Нажмите EN/RESET на плате или проверьте, что загружена версия с интерфейсом.')
            self.wait_started = 0
        self.after(80, self.poll)

    def close(self):
        self.password.set('')
        self.session.password = ''
        if self.worker:
            self.worker.stop()
            self.worker.thread.join(timeout=2.5)
        self.destroy()


if __name__ == '__main__':
    App().mainloop()
