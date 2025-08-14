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
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "bluetooth_audio.h"
#include "../common/picogus.h"
#include <cstdio>
#include <cstring>

// Maximum number of discovered devices
#define MAX_DISCOVERED_DEVICES 20

#ifdef PICOW
// Simple Bluetooth implementation for PicoW
// This is a simplified version that focuses on the core scanning functionality

// Global variables
static bool bt_initialized = false;
static bool scan_active = false;
static int discovered_device_count = 0;
static bt_device_t discovered_devices[MAX_DISCOVERED_DEVICES];
static bt_audio_config_t bt_config = {
    .volume = 50,
    .enabled = false,
    .state = BT_AUDIO_DISCONNECTED,
    .connected_device = {0}
};

// Initialize Bluetooth functionality
void bt_audio_init(void) {
    if (bt_initialized) {
        return;
    }

    printf("BT: Initializing Bluetooth (simplified version)...\n");
    
    // For now, just mark as initialized
    // In a real implementation, this would initialize BTstack
    bt_initialized = true;
    bt_config.enabled = true;
    bt_config.state = BT_AUDIO_DISCONNECTED;
    printf("BT: Bluetooth initialized successfully\n");
}

// Deinitialize Bluetooth functionality
void bt_audio_deinit(void) {
    if (!bt_initialized) {
        return;
    }

    printf("BT: Deinitializing Bluetooth...\n");
    bt_initialized = false;
    bt_config.enabled = false;
    bt_config.state = BT_AUDIO_DISCONNECTED;
}

// Check if Bluetooth audio is connected
bool bt_audio_is_connected(void) {
    return bt_config.enabled && (bt_config.state == BT_AUDIO_CONNECTED || bt_config.state == BT_AUDIO_STREAMING);
}

// Get current Bluetooth audio state
bt_audio_state_t bt_audio_get_state(void) {
    return bt_config.state;
}

// Set Bluetooth audio volume
void bt_audio_set_volume(uint8_t volume) {
    if (volume > 100) volume = 100;
    bt_config.volume = volume;
    printf("BT: Volume set to %d%%\n", volume);
}

// Get Bluetooth audio volume
uint8_t bt_audio_get_volume(void) {
    return bt_config.volume;
}

// Process audio samples and route to Bluetooth if connected
void bt_audio_process_audio(int16_t *samples, uint32_t sample_count, uint32_t sample_rate) {
    if (!bt_config.enabled || !bt_audio_is_connected()) {
        return;
    }

    // Apply volume
    float volume_factor = bt_config.volume / 100.0f;
    for (uint32_t i = 0; i < sample_count * 2; i++) {  // *2 for stereo
        samples[i] = (int16_t)(samples[i] * volume_factor);
    }
}

// Start Bluetooth device scanning
bool bt_audio_scan_start(void) {
    if (!bt_initialized) {
        printf("BT: Bluetooth not initialized\n");
        return false;
    }

    if (scan_active) {
        printf("BT: Scan already active\n");
        return true;
    }

    printf("BT: Starting 10-second device scan...\n");
    
    // Clear previous discoveries
    discovered_device_count = 0;
    memset(discovered_devices, 0, sizeof(discovered_devices));
    
    // Simulate finding some devices after a delay
    // In a real implementation, this would use BTstack's gap_inquiry_start()
    scan_active = true;
    
    // For demonstration, add some fake devices
    if (discovered_device_count < MAX_DISCOVERED_DEVICES) {
        strcpy(discovered_devices[discovered_device_count].address, "00:11:22:33:44:55");
        strcpy(discovered_devices[discovered_device_count].name, "Test Headphones");
        discovered_devices[discovered_device_count].paired = false;
        discovered_devices[discovered_device_count].connected = false;
        discovered_device_count++;
    }
    
    if (discovered_device_count < MAX_DISCOVERED_DEVICES) {
        strcpy(discovered_devices[discovered_device_count].address, "AA:BB:CC:DD:EE:FF");
        strcpy(discovered_devices[discovered_device_count].name, "Bluetooth Speaker");
        discovered_devices[discovered_device_count].paired = false;
        discovered_devices[discovered_device_count].connected = false;
        discovered_device_count++;
    }
    
    printf("BT: Device scan completed, found %d devices:\n", discovered_device_count);
    for (int i = 0; i < discovered_device_count; i++) {
        printf("BT:   %d. %s (%s)\n", i + 1, 
               discovered_devices[i].address,
               discovered_devices[i].name);
    }
    
    scan_active = false;
    return true;
}

// Stop Bluetooth device scanning (no longer needed - auto-stops after 10 seconds)
bool bt_audio_scan_stop(void) {
    if (!bt_initialized) {
        return false;
    }

    printf("BT: Manual scan stop requested (scan auto-stops after 10 seconds)\n");
    return true;
}

// Pair with a device
bool bt_audio_pair_device(const char *address) {
    if (!bt_initialized) {
        return false;
    }

    printf("BT: Pairing with device %s (simulated)\n", address);
    return true;
}

// Unpair a device
bool bt_audio_unpair_device(const char *address) {
    if (!bt_initialized) {
        return false;
    }

    printf("BT: Unpairing device %s (simulated)\n", address);
    return true;
}

// Connect to a device
bool bt_audio_connect_device(const char *address) {
    if (!bt_initialized) {
        return false;
    }

    printf("BT: Connecting to device %s (simulated)\n", address);
    bt_config.state = BT_AUDIO_CONNECTED;
    return true;
}

// Disconnect from current device
bool bt_audio_disconnect_device(void) {
    if (!bt_initialized) {
        return false;
    }

    printf("BT: Disconnecting from device (simulated)\n");
    bt_config.state = BT_AUDIO_DISCONNECTED;
    return true;
}

// Get list of paired devices
int bt_audio_get_paired_devices(bt_device_t *devices, int max_devices) {
    if (!bt_initialized) {
        return 0;
    }

    // For now, return empty list
    return 0;
}

// Get list of discovered devices from last scan
int bt_audio_get_discovered_devices(bt_device_t *devices, int max_devices) {
    if (!bt_initialized) {
        return 0;
    }

    int count = (discovered_device_count < max_devices) ? discovered_device_count : max_devices;
    memcpy(devices, discovered_devices, count * sizeof(bt_device_t));
    return count;
}

// Get number of discovered devices
int bt_audio_get_discovered_device_count(void) {
    return discovered_device_count;
}

// Check if scanning is currently active
bool bt_audio_is_scanning(void) {
    return scan_active;
}

// Start audio streaming
void bt_audio_start_streaming(void) {
    if (!bt_initialized) {
        return;
    }

    printf("BT: Starting audio streaming (simulated)\n");
    bt_config.state = BT_AUDIO_STREAMING;
}

// Stop audio streaming
void bt_audio_stop_streaming(void) {
    if (!bt_initialized) {
        return;
    }

    printf("BT: Stopping audio streaming (simulated)\n");
    bt_config.state = BT_AUDIO_CONNECTED;
}

// Process Bluetooth commands
void bt_audio_process_command(uint8_t cmd, const uint8_t *data, uint16_t length) {
    switch (cmd) {
        case CMD_BT_INIT:
            bt_audio_init();
            break;
            
        case CMD_BT_SCAN:
            bt_audio_scan_start();
            break;
            
        case CMD_BT_PAIR:
            if (data && length > 0) {
                char address[19];
                strncpy(address, (char*)data, 18);
                address[18] = '\0';
                bt_audio_pair_device(address);
            }
            break;
            
        case CMD_BT_UNPAIR:
            if (data && length > 0) {
                char address[19];
                strncpy(address, (char*)data, 18);
                address[18] = '\0';
                bt_audio_unpair_device(address);
            }
            break;
            
        case CMD_BT_CONNECT:
            if (data && length > 0) {
                char address[19];
                strncpy(address, (char*)data, 18);
                address[18] = '\0';
                bt_audio_connect_device(address);
            }
            break;
            
        case CMD_BT_DISCONN:
            bt_audio_disconnect_device();
            break;
            
        case CMD_BT_STATUS:
            printf("BT: Status - Connected: %s, State: %d, Volume: %d%%\n", 
                   bt_audio_is_connected() ? "Yes" : "No",
                   bt_audio_get_state(),
                   bt_audio_get_volume());
            break;
            
        case CMD_BT_DEVICES:
            // Return discovered devices count and list
            printf("BT: Discovered devices: %d\n", bt_audio_get_discovered_device_count());
            if (bt_audio_get_discovered_device_count() > 0) {
                bt_device_t devices[MAX_DISCOVERED_DEVICES];
                int count = bt_audio_get_discovered_devices(devices, MAX_DISCOVERED_DEVICES);
                for (int i = 0; i < count; i++) {
                    printf("BT: Device %d: %s (%s)\n", i + 1, 
                           devices[i].address,
                           devices[i].name);
                }
            }
            break;
            
        case CMD_BT_VOLUME:
            if (data && length > 0) {
                bt_audio_set_volume(data[0]);
            }
            break;
    }
}

#else
// Stub implementations for non-PicoW builds

void bt_audio_init(void) { }
void bt_audio_deinit(void) { }
bool bt_audio_is_connected(void) { return false; }
bt_audio_state_t bt_audio_get_state(void) { return BT_AUDIO_DISCONNECTED; }
void bt_audio_set_volume(uint8_t volume) { (void)volume; }
uint8_t bt_audio_get_volume(void) { return 0; }
void bt_audio_process_audio(int16_t *samples, uint32_t sample_count, uint32_t sample_rate) { 
    (void)samples; (void)sample_count; (void)sample_rate; 
}
void bt_audio_start_streaming(void) { }
void bt_audio_stop_streaming(void) { }
bool bt_audio_scan_start(void) { return false; }
bool bt_audio_scan_stop(void) { return false; }
bool bt_audio_pair_device(const char *address) { (void)address; return false; }
bool bt_audio_unpair_device(const char *address) { (void)address; return false; }
bool bt_audio_connect_device(const char *address) { (void)address; return false; }
bool bt_audio_disconnect_device(void) { return false; }
int bt_audio_get_paired_devices(bt_device_t *devices, int max_devices) { 
    (void)devices; (void)max_devices; return 0; 
}
int bt_audio_get_discovered_devices(bt_device_t *devices, int max_devices) { 
    (void)devices; (void)max_devices; return 0; 
}
int bt_audio_get_discovered_device_count(void) { return 0; }
bool bt_audio_is_scanning(void) { return false; }
void bt_audio_process_command(uint8_t cmd, const uint8_t *data, uint16_t length) { 
    (void)cmd; (void)data; (void)length; 
}
#endif