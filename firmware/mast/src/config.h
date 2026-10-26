#pragma once

#include <ETH.h>
#include <stdint.h>

namespace cfg {

// ---- Network (the laptop's Ethernet port should be 192.168.77.1/24) ----
inline const IPAddress MAST_IP(192, 168, 77, 2);
inline const IPAddress GATEWAY(192, 168, 77, 1);
inline const IPAddress SUBNET(255, 255, 255, 0);

// ---- CRSF link to the ELRS TX module ----
// ELRS TX modules auto-detect the baud rate. 400000 is the EdgeTX default and works everywhere;
// 921600 or 1870000 allow higher packet rates (F1000).
constexpr uint32_t CRSF_BAUD = 400000;

// Frame period used until the module reports its own timing (2000 us = 500 Hz).
constexpr uint32_t DEFAULT_PERIOD_US = 2000;

// If no valid RC packet arrives from the GCS for this long, stop CRSF output.
// The ELRS link then drops and the receiver on the drone triggers failsafe.
constexpr int64_t FAILSAFE_TIMEOUT_US = 200 * 1000;

// Packets older than the newest one are dropped, unless the stream was idle this long
// (the GCS app was restarted and its sequence counter reset).
constexpr int64_t SEQ_RESET_IDLE_US = 500 * 1000;

constexpr uint32_t STATUS_INTERVAL_US = 100 * 1000;

// ---- Board pinout ----
#if defined(WGCS_BOARD_OLIMEX_POE)
constexpr uint8_t ETH_ADDR = 0;
constexpr int ETH_POWER_PIN = 12;
constexpr int ETH_MDC_PIN = 23;
constexpr int ETH_MDIO_PIN = 18;
constexpr eth_clock_mode_t ETH_CLK = ETH_CLOCK_GPIO17_OUT;
constexpr int CRSF_TX_PIN = 4;   // -> 1 kOhm -> module CRSF/S.Port pin
constexpr int CRSF_RX_PIN = 36;  // direct to module CRSF/S.Port pin (input-only GPIO)
constexpr int VRX_CLK_PIN = 32;  // SteadyView SPI (see docs/hardware.md)
constexpr int VRX_DATA_PIN = 33;
constexpr int VRX_CS_PIN = 13;
#elif defined(WGCS_BOARD_WT32_ETH01)
constexpr uint8_t ETH_ADDR = 1;
constexpr int ETH_POWER_PIN = 16;
constexpr int ETH_MDC_PIN = 23;
constexpr int ETH_MDIO_PIN = 18;
constexpr eth_clock_mode_t ETH_CLK = ETH_CLOCK_GPIO0_IN;
constexpr int CRSF_TX_PIN = 17;
constexpr int CRSF_RX_PIN = 5;
constexpr int VRX_CLK_PIN = 14;  // avoid strapping pins (0, 2, 12, 15) for these
constexpr int VRX_DATA_PIN = 4;
constexpr int VRX_CS_PIN = 33;
#else
#error "Select a board: -DWGCS_BOARD_OLIMEX_POE or -DWGCS_BOARD_WT32_ETH01"
#endif

}  // namespace cfg
