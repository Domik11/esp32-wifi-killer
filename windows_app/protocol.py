"""USB-протокол: JSON-события ESP32 и переходы экранного мастера."""
import json
from dataclasses import dataclass, field


def decode_event(line):
    marker = line.find('@ESP32 ')
    if marker < 0:
        return None
    try:
        value = json.loads(line[marker + 7:])
        if isinstance(value, dict) and isinstance(value.get('event'), str):
            return value
    except (ValueError, TypeError):
        pass
    return None


def valid_password(value):
    raw = value.encode('utf-8')
    return (8 <= len(raw) <= 63 or
            len(raw) == 64 and all(c in '0123456789abcdefABCDEF' for c in value)) and '\n' not in value and '\r' not in value


@dataclass
class Session:
    stage: str = 'offline'
    networks: dict = field(default_factory=dict)
    devices: dict = field(default_factory=dict)
    password: str = field(default='', repr=False)
    device_name: str = ''
    choice_pending: str = ''

    def accept(self, event):
        """Возвращает только ожидаемый платой ответ; не отправляет пароль заранее."""
        kind = event['event']
        if kind == 'hello':
            self.stage = 'boot'
            self.networks.clear()
            self.devices.clear()
            self.password = ''
            self.choice_pending = ''
        elif kind == 'wifi_scan':
            self.stage = 'wifi_scan'
            self.networks.clear()
            self.devices.clear()
            self.password = ''
            self.choice_pending = ''
        elif kind == 'wifi' and isinstance(event.get('index'), int):
            self.networks[event['index']] = event
        elif kind == 'device_scan':
            self.stage = 'device_scan'
            self.devices.clear()
        elif kind == 'device' and isinstance(event.get('index'), int):
            self.devices[event['index']] = event
        elif kind == 'request':
            self.stage = event.get('field', '')
            if self.stage == 'wifi_password' and self.choice_pending == 'wifi':
                result = self.password
                self.password = ''
                self.choice_pending = ''
                return result
            if self.stage == 'device_name' and self.choice_pending == 'device':
                self.choice_pending = ''
                return self.device_name
        elif kind == 'wifi_connecting':
            self.stage = 'connecting'
            self.password = ''
            self.choice_pending = ''
        elif kind == 'wifi_connected':
            self.stage = 'device_scan'
        elif kind == 'saved':
            self.stage = 'saved'
        elif kind == 'ready':
            self.stage = 'ready'
        elif kind == 'error':
            self.password = ''
            self.choice_pending = ''
        return None

    def choose_wifi(self, index, password):
        if self.stage != 'wifi_choice' or index not in self.networks:
            raise ValueError('Дождитесь списка Wi-Fi и выберите сеть.')
        if not self.networks[index].get('open') and not valid_password(password):
            raise ValueError('Пароль: 8–63 байта или 64 шестнадцатеричных символа.')
        self.password = '' if self.networks[index].get('open') else password
        self.choice_pending = 'wifi'
        self.stage = 'connecting'
        return str(index)

    def choose_device(self, index, name):
        if self.stage != 'device_choice' or index not in self.devices:
            raise ValueError('Дождитесь списка устройств и выберите строку.')
        if '\n' in name or '\r' in name or len(name.encode('utf-8')) > 128:
            raise ValueError('Имя: не более 128 байт, без перевода строки.')
        self.device_name = name.strip()
        self.choice_pending = 'device'
        self.stage = 'saving'
        return str(index)
