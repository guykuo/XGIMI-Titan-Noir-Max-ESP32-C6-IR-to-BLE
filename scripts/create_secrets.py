#!/usr/bin/env python3
"""Create an ignored ESPHome secrets file from a captured XGIMI wake token."""

from __future__ import annotations

import argparse
import base64
import re
import secrets
from pathlib import Path


OUTPUT = Path(__file__).resolve().parents[1] / "secrets.yaml"


def parse_token(value: str) -> bytes:
    cleaned = re.sub(r"0x", "", value, flags=re.IGNORECASE)
    cleaned = re.sub(r"[^0-9A-Fa-f]", "", cleaned)
    if len(cleaned) != 30:
        raise argparse.ArgumentTypeError(
            "the wake token must contain exactly 15 hexadecimal bytes"
        )
    return bytes.fromhex(cleaned)


def render(token: bytes) -> str:
    api_key = base64.b64encode(secrets.token_bytes(32)).decode("ascii")
    ota_password = secrets.token_urlsafe(24)
    fallback_password = secrets.token_urlsafe(18)
    token_lines = "\n".join(f"  - 0x{byte:02X}" for byte in token)
    return (
        f'api_encryption_key: "{api_key}"\n'
        f'ota_password: "{ota_password}"\n'
        f'fallback_ap_password: "{fallback_password}"\n'
        f"xgimi_wake_token:\n{token_lines}\n"
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create secrets.yaml for the XGIMI Atom Lite firmware"
    )
    parser.add_argument("--token", required=True, type=parse_token)
    args = parser.parse_args()

    if OUTPUT.exists():
        raise SystemExit(f"Refusing to overwrite existing file: {OUTPUT}")
    OUTPUT.write_text(render(args.token), encoding="utf-8")
    print(f"Created {OUTPUT}")
    print("Keep this file private; it contains the wake token and device credentials.")


if __name__ == "__main__":
    main()
