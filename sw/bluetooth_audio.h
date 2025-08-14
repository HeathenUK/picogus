/*
 *  Copyright (C) 2022-2025  Ian Scott
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

// Bluetooth audio states
typedef enum {
    BT_AUDIO_DISCONNECTED = 0,
    BT_AUDIO_CONNECTING,
    BT_AUDIO_CONNECTED,
    BT_AUDIO_STREAMING
} bt_audio_state_t;

// Bluetooth device info
typedef struct {
    char address[18];  // MAC address as string (e.g., "00:11:22:33:44:55")
    char name[32];     // Device name
    bool paired;
    bool connected;
} bt_device_t;

// Bluetooth audio configuration
typedef struct {
    uint8_t volume;           // Volume level (0-100)
    bool enabled;             // Whether Bluetooth audio is enabled
    bt_audio_state_t state;   // Current connection state
    bt_device_t connected_device;
} bt_audio_config_t;

// Function prototypes
void bt_audio_init(void);
void bt_audio_deinit(void);
bool bt_audio_is_connected(void);
bt_audio_state_t bt_audio_get_state(void);
void bt_audio_set_volume(uint8_t volume);
uint8_t bt_audio_get_volume(void);

// Audio routing functions
void bt_audio_process_audio(int16_t *samples, uint32_t sample_count, uint32_t sample_rate);
void bt_audio_start_streaming(void);
void bt_audio_stop_streaming(void);

// Device management
bool bt_audio_scan_start(void);
bool bt_audio_scan_stop(void);
bool bt_audio_pair_device(const char *address);
bool bt_audio_unpair_device(const char *address);
bool bt_audio_connect_device(const char *address);
bool bt_audio_disconnect_device(void);
int bt_audio_get_paired_devices(bt_device_t *devices, int max_devices);
int bt_audio_get_discovered_devices(bt_device_t *devices, int max_devices);
int bt_audio_get_discovered_device_count(void);
bool bt_audio_is_scanning(void);

// Command processing (for pgusinit integration)
void bt_audio_process_command(uint8_t cmd, const uint8_t *data, uint16_t length);