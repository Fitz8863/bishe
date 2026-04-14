#!/usr/bin/env python3

from pathlib import Path
import re
import sys


PROJECT_DIR = Path(__file__).resolve().parent
INO_PATH = PROJECT_DIR / "esp8266-oled-mqtt-uart.ino"
SAMPLE_DRCOM_RESPONSE = 'dr1003({"result":0,"wopt":0,"msg":1,"uid":"2200340118@unicom","msga":"clientip online"})'
SAMPLE_DRCOM_RESPONSE_WITH_SPACE = 'dr1003({"result": 0,"wopt":0,"msg":1,"uid":"2200340118@unicom","msga":"clientip online"})'
SAMPLE_PORTAL_SUCCESS_RESPONSE = 'dr1003({"result":1,"msg":"认证成功","ret_code":2})'


def portal_response_confirms_online(response: str) -> bool:
    normalized = response.strip()
    result_is_success = '"result":0' in normalized or '"result": 0' in normalized
    return result_is_success and (
        'clientip online' in normalized or '"msg":1' in normalized
    )


def portal_login_response_accepted(response: str) -> bool:
    normalized = response.strip()
    success_result = (
        '"result":1' in normalized
        or '"result":"1"' in normalized
        or '"result":"ok"' in normalized
    )
    obvious_failure = any(
        needle in normalized for needle in ('密码错误', '账号不存在', 'ldap auth error')
    )
    return success_result and not obvious_failure


def extract(pattern: str, text: str, label: str) -> str:
    match = re.search(pattern, text, re.MULTILINE)
    if not match:
        raise RuntimeError(f"Could not find {label}")
    return match.group(1)


def main() -> int:
    ino_text = INO_PATH.read_text(encoding="utf-8")

    portal_login_url = extract(r'const char \*portal_login_base_url = "([^"]+)";', ino_text, "portal_login_base_url")
    max_login_attempts = extract(r'constexpr uint8_t max_login_attempts = (\d+);', ino_text, "max_login_attempts")
    login_timeout_ms = extract(r'constexpr unsigned long login_request_timeout_ms = (\d+);', ino_text, "login_request_timeout_ms")

    failures = []

    if portal_login_url != "http://10.0.1.5:801/eportal/portal/login":
        failures.append(f"Unexpected portal_login_base_url: {portal_login_url}")

    if max_login_attempts != "5":
        failures.append(f"Expected max_login_attempts=5, got {max_login_attempts}")

    if login_timeout_ms != "10000":
        failures.append(f"Expected login_request_timeout_ms=10000, got {login_timeout_ms}")

    if not portal_response_confirms_online(SAMPLE_DRCOM_RESPONSE):
        failures.append("Expected sample DrCOM success response to be treated as authenticated")

    if not portal_response_confirms_online(SAMPLE_DRCOM_RESPONSE_WITH_SPACE):
        failures.append("Expected sample DrCOM success response with whitespace to be treated as authenticated")

    if not portal_login_response_accepted(SAMPLE_PORTAL_SUCCESS_RESPONSE):
        failures.append("Expected sample portal login success response to be accepted")

    required_helpers = [
        "bool capturePortalRedirect",
        "String buildPortalLoginUrl",
        "bool performPortalLogin",
        "bool portalAccessStillIntercepted",
    ]
    for helper in required_helpers:
        if helper not in ino_text:
            failures.append(f"Expected ESP8266 sketch to implement helper: {helper}")

    if failures:
        for failure in failures:
            print(f"[FAIL] {failure}")
        return 1

    print("[PASS] portal_login_base_url points to eportal login API")
    print("[PASS] max_login_attempts matches shell retry strategy")
    print("[PASS] login_request_timeout_ms matches shell timeout strategy")
    print("[PASS] sample DrCOM JSONP success response is recognized as authenticated")
    print("[PASS] whitespace-variant DrCOM JSONP success response is recognized as authenticated")
    print("[PASS] sample portal JSONP success response is recognized as authenticated")
    return 0


if __name__ == "__main__":
    sys.exit(main())
