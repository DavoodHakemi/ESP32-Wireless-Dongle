from __future__ import annotations

import builtins
import time
from typing import Any


_INSTALLED = False
_ORIGINAL_PRINT = builtins.print
_ORIGINAL_INPUT = builtins.input
_START_MONOTONIC = time.monotonic()


def _elapsed_ms() -> int:
    return int((time.monotonic() - _START_MONOTONIC) * 1000.0)


def _is_protocol_log_line(text: str) -> bool:
    stripped = text.lstrip()
    return stripped.startswith("[PC,") or stripped.startswith("[ESP,")


def _write_line(
    text: str,
    *,
    log_type: str,
    end: str,
    file: Any,
    flush: bool,
) -> None:
    if _is_protocol_log_line(text):
        _ORIGINAL_PRINT(text, end=end, file=file, flush=flush)
    else:
        _ORIGINAL_PRINT(
            f"[PC, {log_type}, {_elapsed_ms()} ms] {text}",
            end=end,
            file=file,
            flush=flush,
        )


def _console_print(
    *values: Any,
    sep: str = " ",
    end: str = "\n",
    file: Any = None,
    flush: bool = False,
) -> None:
    target = builtins.sys.stdout if file is None else file
    text = sep.join(str(value) for value in values)
    lines = text.splitlines()

    if not lines:
        lines = [""]

    for index, line in enumerate(lines):
        line_end = end if index == len(lines) - 1 else "\n"
        _write_line(
            line,
            log_type="UI",
            end=line_end,
            file=target,
            flush=flush,
        )


def _console_input(prompt: str = "") -> str:
    if prompt:
        _write_line(
            str(prompt).rstrip("\r\n"),
            log_type="Prompt",
            end="",
            file=builtins.sys.stdout,
            flush=True,
        )
    return _ORIGINAL_INPUT()


def install_console_contract() -> None:
    """Ensure every host-side terminal line follows the PC/ESP log contract."""
    global _INSTALLED

    if _INSTALLED:
        return

    builtins.print = _console_print
    builtins.input = _console_input
    _INSTALLED = True


__all__ = ["install_console_contract"]
