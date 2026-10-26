import unittest
from protocol import Session, decode_event, valid_password


class ProtocolTests(unittest.TestCase):
    def test_json_after_interleaved_log_and_unicode(self):
        self.assertEqual(decode_event('Ping: @ESP32 {"event":"wifi","ssid":"Тест \\\"сеть\\\""}')['ssid'], 'Тест "сеть"')
        for text in ('normal log', '@ESP32 broken', '@ESP32 []', '@ESP32 {"event":2}'):
            self.assertIsNone(decode_event(text))

    def test_credentials_wait_for_password_request_and_are_cleared(self):
        session = Session()
        session.accept({'event': 'wifi_scan'})
        session.accept({'event': 'wifi', 'index': 1, 'open': False})
        session.accept({'event': 'request', 'field': 'wifi_choice'})
        self.assertEqual(session.choose_wifi(1, 'test-only-pass'), '1')
        self.assertNotIn('test-only-pass', repr(session))
        self.assertEqual(session.accept({'event': 'request', 'field': 'wifi_password'}), 'test-only-pass')
        self.assertEqual(session.password, '')
        self.assertIsNone(session.accept({'event': 'request', 'field': 'wifi_password'}))

    def test_stale_credentials_not_reused_after_error_or_reset(self):
        for event in ({'event': 'hello'}, {'event': 'wifi_scan'}, {'event': 'error'}):
            session = Session(password='old-pass', choice_pending='wifi')
            session.accept(event)
            self.assertEqual(session.password, '')
            self.assertIsNone(session.accept({'event': 'request', 'field': 'wifi_password'}))

    def test_open_network_needs_no_password(self):
        session = Session(stage='wifi_choice', networks={2: {'open': True}})
        self.assertEqual(session.choose_wifi(2, ''), '2')
        session.accept({'event': 'wifi_connecting'})
        self.assertEqual(session.password, '')

    def test_wrong_stage_and_unknown_device_rejected(self):
        session = Session()
        with self.assertRaises(ValueError): session.choose_wifi(1, 'test-pass')
        with self.assertRaises(ValueError): session.choose_device(1, '')
        session.stage = 'device_choice'
        with self.assertRaises(ValueError): session.choose_device(7, '')

    def test_blank_name_is_sent_only_when_requested(self):
        session = Session(stage='device_choice', devices={3: {}})
        self.assertEqual(session.choose_device(3, ''), '3')
        self.assertEqual(session.accept({'event': 'request', 'field': 'device_name'}), '')
        self.assertIsNone(session.accept({'event': 'request', 'field': 'device_name'}))

    def test_byte_limits_and_line_injection(self):
        self.assertTrue(valid_password('a'*63))
        self.assertTrue(valid_password('f'*64))
        self.assertFalse(valid_password('g'*64))
        self.assertFalse(valid_password('abc\ndefghi'))
        session = Session(stage='device_choice', devices={1: {}})
        with self.assertRaises(ValueError): session.choose_device(1, 'bad\nname')
        with self.assertRaises(ValueError): session.choose_device(1, 'Я'*65)


if __name__ == '__main__':
    unittest.main()
