#!/usr/bin/env python3
"""Observe XGIMI rolling-counter advertisements without printing wake tokens."""

from __future__ import annotations

import argparse
import asyncio
from collections import Counter
from datetime import datetime
from hashlib import sha256
import time

from bleak import BleakScanner


XGIMI_COMPANY_ID = 0x0046
EXPECTED_PAYLOAD_BYTES = 16


def format_ranges(values: list[int]) -> str:
    if not values:
        return "none"
    ranges: list[str] = []
    start = previous = values[0]
    for value in values[1:]:
        if value == previous + 1:
            previous = value
            continue
        ranges.append(f"{start}" if start == previous else f"{start}-{previous}")
        start = previous = value
    ranges.append(f"{start}" if start == previous else f"{start}-{previous}")
    return ", ".join(ranges)


class CoverageTracker:
    def __init__(self) -> None:
        self.sweep = 0
        self.seen: Counter[int] = Counter()
        self.previous: int | None = None
        self.has_wrapped = False
        self.started = time.monotonic()

    def record(self, counter: int) -> None:
        if self.previous == counter:
            self.seen[counter] += 1
            return

        if (
            self.previous is not None
            and self.previous > 192
            and counter < 64
        ):
            self.report("full-cycle" if self.has_wrapped else "initial-partial")
            self.has_wrapped = True
            self.seen.clear()
            self.started = time.monotonic()

        self.seen[counter] += 1
        self.previous = counter

    def report(self, window: str) -> None:
        if not self.seen:
            return
        self.sweep += 1
        missing = [counter for counter in range(256) if counter not in self.seen]
        duplicates = sum(count - 1 for count in self.seen.values())
        print(
            f"COVERAGE window={self.sweep} type={window} "
            f"coverage={'complete' if not missing else 'incomplete'} "
            f"unique={len(self.seen)}/256 "
            f"missing={len(missing)} duplicates={duplicates} "
            f"elapsed={time.monotonic() - self.started:.3f}s "
            f"missing_values={format_ranges(missing)}",
            flush=True,
        )


async def observe(
    duration: float,
    coverage: bool,
    coverage_only: bool,
    name_filter: str | None,
    excluded_device: str | None,
    details: bool,
) -> None:
    tracker = CoverageTracker()
    detailed_packets: set[tuple[str, int]] = set()

    def on_advertisement(device, advertisement_data) -> None:
        if excluded_device is not None and device.address.casefold() == excluded_device.casefold():
            return
        payload = advertisement_data.manufacturer_data.get(XGIMI_COMPANY_ID)
        if payload is None or len(payload) != EXPECTED_PAYLOAD_BYTES:
            return

        counter, token = payload[0], bytes(payload[1:])
        name = advertisement_data.local_name or device.name or "(unnamed)"
        if name_filter is not None and name != name_filter:
            return
        if coverage:
            tracker.record(counter)
        if coverage_only:
            return
        token_id = sha256(token).hexdigest()[:8]
        timestamp = datetime.now().astimezone().isoformat(timespec="milliseconds")
        print(
            f"{timestamp}  counter={counter:3d} (0x{counter:02X})  "
            f"name={name!r}  device={device.address}  token_id={token_id}",
            flush=True,
        )
        detail_key = (device.address.casefold(), counter)
        if details and detail_key not in detailed_packets:
            detailed_packets.add(detail_key)
            platform_fields: list[str] = []
            for item in advertisement_data.platform_data:
                if not isinstance(item, dict):
                    continue
                for key, value in item.items():
                    key_text = str(key)
                    if "Manufacturer" in key_text:
                        platform_fields.append(f"{key_text}=<redacted {len(value)} bytes>")
                    elif "ServiceData" in key_text:
                        platform_fields.append(f"{key_text}=<service-data keys only>")
                    else:
                        platform_fields.append(f"{key_text}={value!r}")
            print(
                "DETAIL "
                f"counter=0x{counter:02X} device={device.address} "
                f"service_uuids={advertisement_data.service_uuids!r} "
                f"service_data_keys={list(advertisement_data.service_data)!r} "
                f"manufacturer_fields="
                f"{[(company, len(data)) for company, data in advertisement_data.manufacturer_data.items()]!r} "
                f"tx_power={advertisement_data.tx_power!r} rssi={advertisement_data.rssi} "
                f"platform={platform_fields!r}",
                flush=True,
            )

    print(
        "Observing XGIMI company-ID 0x0046 advertisements. The stable wake token "
        "is represented only by a non-secret hash prefix."
    )
    async with BleakScanner(detection_callback=on_advertisement):
        await asyncio.sleep(duration)
    if coverage:
        tracker.report("full-cycle" if len(tracker.seen) == 256 else "trailing-partial")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Observe XGIMI wake counters from an original remote or clone"
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=120.0,
        help="scan duration in seconds (default: 120)",
    )
    parser.add_argument(
        "--coverage",
        action="store_true",
        help="report distinct and missing counters at each observed 255-to-0 wrap",
    )
    parser.add_argument(
        "--coverage-only",
        action="store_true",
        help="suppress individual advertisement lines (implies --coverage)",
    )
    parser.add_argument(
        "--name",
        help="only include advertisements whose local name matches exactly",
    )
    parser.add_argument(
        "--exclude-device",
        help="ignore one operating-system BLE device identifier (case-insensitive)",
    )
    parser.add_argument(
        "--details",
        action="store_true",
        help="print one token-safe GAP metadata summary per device and counter",
    )
    args = parser.parse_args()
    asyncio.run(
        observe(
            args.duration,
            args.coverage or args.coverage_only,
            args.coverage_only,
            args.name,
            args.exclude_device,
            args.details,
        )
    )


if __name__ == "__main__":
    main()
