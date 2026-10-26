#!/usr/bin/env python3
"""Negative controls for the browser document/challenge evidence gate."""
import json
import unittest

from verify_chromium import renderer_problems


class RendererEvidenceTest(unittest.TestCase):
    challenge = ('0123456789abcdef0123456789abcdef', 123, 456)

    def record(self, **updates):
        row = {'nonce': self.challenge[0], 'answer': 123 * 456,
               'href': 'https://www.yahoo.com/', 'uri': 'https://www.yahoo.com/',
               'ready': 'complete', 'secure': True, 'title': 'Yahoo | Mail',
               'text_length': 1000, 'http_status': 200}
        row.update(updates)
        return b'[CHROMIUM_PAGE] ' + json.dumps(row).encode() + b'\n'

    def test_observation_with_matching_fresh_challenge(self):
        self.assertEqual([], renderer_problems(self.record(), self.challenge))

    def test_integral_double_serialization_is_accepted(self):
        challenge = (self.challenge[0], 60000, 50000)
        self.assertEqual([], renderer_problems(self.record(answer=3000000000.0), challenge))

    def test_echoed_command_does_not_prove_execution(self):
        command = f'/bin/chromium_page_probe {self.challenge[0]} 123 456\r\n'.encode()
        self.assertTrue(renderer_problems(command, self.challenge))

    def test_error_page_with_yahoo_url_and_title_fails(self):
        self.assertTrue(renderer_problems(self.record(uri='chrome-error://chromewebdata/'), self.challenge))

    def test_stale_nonce_fails(self):
        self.assertTrue(renderer_problems(self.record(nonce='f' * 32), self.challenge))

    def test_wrong_answer_fails(self):
        self.assertTrue(renderer_problems(self.record(answer=123 + 456), self.challenge))

    def test_incomplete_insecure_blank_or_wrong_document_fails(self):
        for update in ({'ready': 'loading'}, {'secure': False}, {'text_length': 0},
                       {'href': 'https://www.yahoo.com.example.invalid/'},
                       {'title': 'Privacy error'}, {'answer': True}, {'http_status': 429}):
            with self.subTest(update=update):
                self.assertTrue(renderer_problems(self.record(**update), self.challenge))

    def test_malformed_observation_fails(self):
        for text in (b'[CHROMIUM_PAGE] {\n', b'[CHROMIUM_PAGE] []\n', b'[CHROMIUM_PAGE] null\n'):
            with self.subTest(text=text):
                self.assertTrue(renderer_problems(text, self.challenge))


if __name__ == '__main__':
    unittest.main()
