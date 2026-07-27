import json
import threading
import time
import unittest
from urllib.error import HTTPError
from urllib.parse import urlencode
from urllib.request import Request, urlopen

from scripts.web_preview import PreviewApplication, create_server, extract_web_page


class WebPreviewTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.application = PreviewApplication(
            available_version="9.9.9", delay_scale=0.01, log_requests=False
        )
        cls.server = create_server("127.0.0.1", 0, cls.application)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.base_url = f"http://127.0.0.1:{cls.server.server_port}"

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=2)

    def request(self, path, method="GET", data=None, token=None):
        headers = {}
        encoded_data = None
        if data is not None:
            encoded_data = urlencode(data).encode("ascii")
            headers["Content-Type"] = "application/x-www-form-urlencoded"
        if token:
            headers["X-CSRF-Token"] = token
        request = Request(
            self.base_url + path,
            method=method,
            data=encoded_data,
            headers=headers,
        )
        with urlopen(request, timeout=2) as response:
            return response.status, response.read(), response.headers

    def state(self):
        _, content, _ = self.request("/api/state")
        return json.loads(content)

    def wait_for_status(self, expected_status):
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            state = self.state()
            if state["otaStatus"] == expected_status:
                return state
            time.sleep(0.01)
        self.fail(f"OTA status did not become {expected_status!r}")

    def test_serves_exact_embedded_web_page(self):
        html = extract_web_page()
        self.assertIn("<!doctype html>", html)
        self.assertIn('id="power"', html)
        self.assertIn("document.hidden ? 15000 : 1000", html)
        self.assertNotIn("setInterval(refresh, 1000)", html)
        _, served_html, headers = self.request("/")
        self.assertEqual(served_html.decode("utf-8"), html)
        self.assertEqual(headers.get_content_type(), "text/html")

    def test_controls_led_through_mock_api(self):
        token = self.state()["csrfToken"]
        status, content, _ = self.request(
            "/api/control",
            method="POST",
            data={"brightness": "73"},
            token=token,
        )
        state = json.loads(content)
        self.assertEqual(status, 200)
        self.assertTrue(state["power"])
        self.assertEqual(state["mode"], "web")
        self.assertEqual(state["outputBrightness"], 73)

    def test_rejects_mutation_without_csrf_token(self):
        with self.assertRaises(HTTPError) as context:
            self.request("/api/ota/check", method="POST")
        self.assertEqual(context.exception.code, 403)
        context.exception.close()

    def test_simulates_complete_ota_flow(self):
        token = self.state()["csrfToken"]
        self.request("/api/ota/check", method="POST", token=token)
        available = self.wait_for_status("available")
        self.assertEqual(available["otaAvailableVersion"], "9.9.9")

        self.request("/api/ota/install", method="POST", token=token)
        updated = self.wait_for_status("up_to_date")
        self.assertEqual(updated["firmwareVersion"], "9.9.9")
        self.assertFalse(updated["otaUpdateAvailable"])


if __name__ == "__main__":
    unittest.main()
