#!/usr/bin/env python3
"""Capture the per-remote XGIMI wake token from BLE advertisements."""

from __future__ import annotations

import argparse
import asyncio
from collections import defaultdict
from dataclasses import dataclass, field

from bleak import BleakScanner


XGIMI_COMPANY_ID = 0x0046
EXPECTED_PAYLOAD_BYTES = 16  # one rolling counter byte plus a 15-byte token


@dataclass
class Candidate:
    names: set[str] = field(default_factory=set)
    device_ids: set[str] = field(default_factory=set)
    counters: set[int] = field(default_factory=set)
    observations: int = 0


def format_yaml(token: bytes) -> str:
    lines = ["xgimi_wake_token:"]
    lines.extend(f"  - 0x{value:02X}" for value in token)
    return "\n".join(lines)


async def capture(duration: float) -> dict[bytes, Candidate]:
    candidates: dict[bytes, Candidate] = defaultdict(Candidate)

    def on_advertisement(device, advertisement_data) -> None:
        payload = advertisement_data.manufacturer_data.get(XGIMI_COMPANY_ID)
        if payload is None or len(payload) != EXPECTED_PAYLOAD_BYTES:
            return

        counter, token = payload[0], bytes(payload[1:])
        candidate = candidates[token]
        candidate.observations += 1
        candidate.counters.add(counter)
        candidate.device_ids.add(str(device.address))
        name = advertisement_data.local_name or device.name
        if name:
            candidate.names.add(name)

        print(
            f"Captured counter 0x{counter:02X}; "
            f"token {token.hex(':').upper()}; "
            f"device {device.address}",
            flush=True,
        )

    async with BleakScanner(detection_callback=on_advertisement):
        await asyncio.sleep(duration)
    return candidates


async def main_async(duration: float) -> int:
    print(
        "Scanning for XGIMI wake advertisements. Keep the projector unplugged "
        "and press the original remote's Power button several times."
    )
    candidates = await capture(duration)
    if not candidates:
        print(
            "\nNo 16-byte XGIMI manufacturer payload was found. Confirm Bluetooth "
            "permission, keep the remote close, press Power again, and retry."
        )
        return 1

    ranked = sorted(
        candidates.items(),
        key=lambda item: (len(item[1].counters), item[1].observations),
        reverse=True,
    )
    token, best = ranked[0]

    print("\nBest candidate:")
    print(f"  names: {', '.join(sorted(best.names)) or '(not advertised)'}")
    print(f"  observations: {best.observations}")
    print(f"  distinct rolling counters: {len(best.counters)}")
    print(f"  stable 15-byte token: {token.hex(':').upper()}")
    if len(best.counters) < 2:
        print(
            "\nWarning: only one rolling-counter value was observed. Run the scan "
            "again and press Power several times before trusting this token."
        )
        return 2

    print("\nPaste this into secrets.yaml:\n")
    print(format_yaml(token))
    return 0


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Capture a Titan Noir Max remote's 15-byte BLE wake token"
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=30.0,
        help="scan duration in seconds (default: 30)",
    )
    args = parser.parse_args()
    raise SystemExit(asyncio.run(main_async(args.duration)))


if __name__ == "__main__":
    main()
