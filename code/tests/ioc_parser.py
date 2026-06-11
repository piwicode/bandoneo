"""Minimal parser for STM32CubeMX .ioc files (Java properties format)."""

import re
from pathlib import Path

SPI_SIGNAL_RE = re.compile(r"^(SPI\d)_(NSS|SCK|MOSI|MISO)$")


def parse_ioc(path: Path) -> dict[str, str]:
    settings = {}
    for line in Path(path).read_text().splitlines():
        if "=" not in line:
            continue
        key, _, value = line.partition("=")
        settings[key] = value
    return settings


def spi_instances(settings: dict) -> dict[str, dict[str, str]]:
    """Map SPI instance name -> its settings with the "SPIx." prefix stripped.

    e.g. {"SPI1": {"Mode": "SPI_MODE_SLAVE", "CRCLength": "SPI_CRC_LENGTH_16BIT", ...}}
    """
    instances: dict[str, dict[str, str]] = {}
    for key, value in settings.items():
        if not key.startswith("SPI"):
            continue
        instance, _, sub_key = key.partition(".")
        instances.setdefault(instance, {})[sub_key] = value
    return instances


def dma_request(settings: dict, name: str) -> dict[str, str] | None:
    """Return {field: value} for the DMA request block "Dma.<name>.<n>.*", or None.

    e.g. dma_request(settings, "SPI1_RX") -> {"Instance": "DMA1_Channel1",
    "Direction": "DMA_PERIPH_TO_MEMORY", "Mode": "DMA_NORMAL", ...}.
    """
    block_re = re.compile(rf"^Dma\.{re.escape(name)}\.\d+\.(.+)$")
    fields = {}
    for key, value in settings.items():
        m = block_re.match(key)
        if m:
            fields[m.group(1)] = value
    return fields or None


def spi_pins(settings: dict, spi: str) -> dict[str, str]:
    """Map signal function -> GPIO pin name for one SPI instance.

    e.g. spi_pins(settings, "SPI1") -> {"NSS": "PA4", "SCK": "PA5", ...}, derived from
    `PA4.Signal=SPI1_NSS`.
    """
    result = {}
    for key, value in settings.items():
        if not key.endswith(".Signal"):
            continue
        m = SPI_SIGNAL_RE.match(value)
        if not m:
            continue
        instance, func = m.groups()
        if instance == spi:
            result[func] = key[: -len(".Signal")]
    return result
