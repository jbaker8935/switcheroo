"""Helpers for inspecting Foenix PGZ images.

This utility mirrors the behaviour of the upstream `pgz-thunk.py` helper that
ships with the Foenix SDK. It prints human readable information about the
segments that were packed into a `.pgz` file so the build logs can show the
load addresses and sizes.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def _read_word(data: bytes) -> int:
    """Interpret a little-endian word of length 3 or 4 bytes."""

    if len(data) == 3:
        data += b"\x00"
    if len(data) != 4:
        raise ValueError("PGZ fields must be 3 or 4 bytes long")
    return struct.unpack("<I", data)[0]


def describe_pgz(pgz_path: Path) -> list[str]:
    """Return a list of human readable lines describing a PGZ image."""

    output: list[str] = []
    with pgz_path.open("rb") as handle:
        signature = handle.read(1)
        if signature == b"Z":
            field_size = 3
            output.append('Signature: "Z" 24-bit addresses')
        elif signature == b"z":
            field_size = 4
            output.append('Signature: "z" 32-bit addresses')
        else:
            raise ValueError("Invalid PGZ signature; expected 'Z' or 'z'")

        segment = 1
        while True:
            address_bytes = handle.read(field_size)
            size_bytes = handle.read(field_size)

            if len(address_bytes) != field_size or len(size_bytes) != field_size:
                raise EOFError("Truncated PGZ file; segment header incomplete")

            address = _read_word(address_bytes)
            size = _read_word(size_bytes)

            if size == 0:
                output.append(
                    f"Segment {segment}: Program entry point at ${address:06x}"
                )
                break

            output.append(
                f"Segment {segment}: Address=${address:06x} Size=${size:06x} ({size})"
            )
            data = handle.read(size)
            if len(data) != size:
                raise EOFError("Truncated PGZ file; segment payload incomplete")
            segment += 1

    return output


def main() -> None:
    parser = argparse.ArgumentParser(description="Inspect Foenix PGZ images")
    parser.add_argument("pgz", type=Path, help="Path to the .pgz file to inspect")
    args = parser.parse_args()

    for line in describe_pgz(args.pgz):
        print(line)


if __name__ == "__main__":
    main()