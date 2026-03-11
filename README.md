# STM32 SPI SD-Card Driver (Lab Project)

## Project Overview
This project is a low-level implementation of the SPI protocol for interfacing with SD memory cards using an STM32 (Nucleo) F446RE microcontroller. The primary goal was to study the SPI interface "from scratch" and understand the initialization nuances of SDHC and SDXC standards without relying on high-level libraries.

<img width="1061" height="455" alt="chrome_NjEGJFX747" src="https://github.com/user-attachments/assets/8c2c42d8-5ded-40c7-93f4-3047d4208347" />


## Project Status: TODO / On Hold
The current implementation successfully performs sector reading, but encounters stability issues during write operations on 64 GB (SDXC) cards due to hardware/physical layer constraints.

### Tech Stack
* MCU: STM32 (Cortex-M4)
* Interface: SPI (Master mode)
* Peripheral: MicroSD slot module with logic level shifters (LVC125A)
* Toolchain: VScode / CMSIS 

## Features Implemented
[x] SPI Master mode initialization and configuration.

[x] SPI mode entry sequence (CMD0, CMD8).

[x] High-Capacity card initialization (ACMD41 with HCS support).

[x] Single sector read functionality (CMD17).

[x] Proper handling of card responses (R1, R3, R7).

## Hardware Limitations & Debugging Notes
Power Supply Instability: SDXC cards (64 GB+) are highly sensitive to voltage drops. Powering the module via a standard USB port (providing ~4.5V) is insufficient for the module's onboard LDO regulator. Stable 5.0V–5.2V or a direct 3.3V feed is required.

Signal Integrity: Low-cost level-shifter modules distort SPI signal edges at higher frequencies. This "ringing" or signal degradation causes the card controller to hang during write operations, resulting in WaitReady Timeout.

Module Compatibility: The specific module used is officially rated for cards up to 32 GB (SDHC). The higher power consumption and timing requirements of 64 GB cards exceed the module's stable operating range.

### Next Steps (Roadmap)
[ ] Replace the active level-shifter module with a passive MicroSD breakout for direct 3.3V connection.

[ ] Add a decoupling capacitor (470 µF) directly across the card's power pins to buffer peak currents.

[ ] Implement multi-block write functionality (CMD25).

[ ] Integrate FatFS library for high-level file system management.

## Key SPI Insights Gained
Initialization Frequency: Must be kept below 400 kHz to ensure compatibility with all card types.
MISO Configuration: Must be set to Pull-up to prevent floating line noise when the card is not selected.
Dummy Clocks: Sending 8 clock pulses (0xFF) after deactivating CS is essential for the card to finalize internal state transitions.
