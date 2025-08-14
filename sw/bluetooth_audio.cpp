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

#include "bluetooth_audio.h"
#include "../common/picogus.h"

#ifdef PICOW
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "pico/btstack_run_loop_async_context.h"
#include "btstack.h"
#include "btstack_config.h"
#include "classic/a2dp_source.h"
#include "classic/avdtp_source.h"
#include "classic/gap.h"
#include "classic/sm.h"

// Global Bluetooth audio configuration
static bt_audio_config_t bt_config = {
    .volume = 50,
    .enabled = false,
    .state = BT_AUDIO_DISCONNECTED,
    .connected_device = {0}
};

// BTstack variables
static bd_addr_t connected_addr;
static bool scanning = false;
static bool pairing_mode = false;

// Audio buffer for A2DP streaming
static int16_t bt_audio_buffer[1024];  // 2 seconds at 44.1kHz stereo
static uint32_t bt_audio_buffer_pos = 0;
static bool bt_audio_streaming = false;

// BTstack event handlers
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("BT: Device disconnected\n");
            bt_config.state = BT_AUDIO_DISCONNECTED;
            memset(&bt_config.connected_device, 0, sizeof(bt_device_t));
            bt_audio_stop_streaming();
            break;

        case A2DP_EVENT_SIGNALING_CONNECTION_ESTABLISHED:
            printf("BT: A2DP connection established\n");
            bt_config.state = BT_AUDIO_CONNECTED;
            break;

        case A2DP_EVENT_SIGNALING_CONNECTION_RELEASED:
            printf("BT: A2DP connection released\n");
            bt_config.state = BT_AUDIO_DISCONNECTED;
            bt_audio_stop_streaming();
            break;

        case A2DP_EVENT_STREAM_ESTABLISHED:
            printf("BT: A2DP stream established\n");
            bt_config.state = BT_AUDIO_STREAMING;
            bt_audio_start_streaming();
            break;

        case A2DP_EVENT_STREAM_RELEASED:
            printf("BT: A2DP stream released\n");
            bt_config.state = BT_AUDIO_CONNECTED;
            bt_audio_stop_streaming();
            break;

        case GAP_EVENT_INQUIRY_RESULT:
            if (scanning) {
                bd_addr_t addr;
                gap_event_inquiry_result_get_bd_addr(packet, addr);
                char addr_str[18];
                btstack_crypto_hex(addr, 6, addr_str);
                printf("BT: Found device %s\n", addr_str);
            }
            break;

        case GAP_EVENT_INQUIRY_COMPLETE:
            if (scanning) {
                printf("BT: Scan complete\n");
                scanning = false;
            }
            break;

        case SM_EVENT_AUTHORIZATION_RESULT:
            if (pairing_mode) {
                printf("BT: Pairing authorized\n");
                pairing_mode = false;
            }
            break;

        default:
            break;
    }
}

// Initialize Bluetooth audio system
void bt_audio_init(void) {
    if (!bt_config.enabled) {
        printf("BT: Initializing Bluetooth audio...\n");
        
        // Initialize CYW43
        if (cyw43_arch_init()) {
            printf("BT: Failed to initialize CYW43\n");
            return;
        }

        // Initialize BTstack
        if (!btstack_cyw43_init(cyw43_arch_async_context())) {
            printf("BT: Failed to initialize BTstack\n");
            return;
        }

        // Setup GAP
        gap_discoverable_control(1);
        gap_set_local_name("PicoGUS Audio");
        gap_set_class_of_device(0x240404);  // Audio device

        // Setup Security Manager
        sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
        sm_set_io_capabilities(SM_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);

        // Setup A2DP Source
        a2dp_source_init();
        a2dp_source_register_packet_handler(packet_handler);

        // Power on Bluetooth
        hci_power_control(HCI_POWER_ON);

        bt_config.enabled = true;
        printf("BT: Bluetooth audio initialized\n");
    }
}

// Deinitialize Bluetooth audio system
void bt_audio_deinit(void) {
    if (bt_config.enabled) {
        printf("BT: Deinitializing Bluetooth audio...\n");
        
        if (bt_audio_is_connected()) {
            bt_audio_disconnect_device();
        }

        btstack_cyw43_deinit(cyw43_arch_async_context());
        cyw43_arch_deinit();
        
        bt_config.enabled = false;
        bt_config.state = BT_AUDIO_DISCONNECTED;
        printf("BT: Bluetooth audio deinitialized\n");
    }
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
    
    if (bt_audio_is_connected()) {
        // Set A2DP volume
        a2dp_source_set_volume(volume);
    }
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

    // Buffer audio for A2DP streaming
    if (bt_audio_streaming && bt_config.state == BT_AUDIO_STREAMING) {
        uint32_t samples_to_copy = sample_count * 2;  // Stereo samples
        if (bt_audio_buffer_pos + samples_to_copy <= sizeof(bt_audio_buffer) / sizeof(int16_t)) {
            memcpy(&bt_audio_buffer[bt_audio_buffer_pos], samples, samples_to_copy * sizeof(int16_t));
            bt_audio_buffer_pos += samples_to_copy;
        }
    }
}

// Start A2DP streaming
void bt_audio_start_streaming(void) {
    if (bt_config.enabled && bt_config.state == BT_AUDIO_CONNECTED) {
        printf("BT: Starting A2DP streaming\n");
        bt_audio_streaming = true;
        bt_audio_buffer_pos = 0;
        
        // Configure A2DP stream
        a2dp_source_set_codec(SBC_CODEC_ID);
        a2dp_source_set_sample_rate(44100);
        a2dp_source_set_channels(2);
        a2dp_source_set_bitpool(53);  // High quality SBC
        
        // Start streaming
        a2dp_source_stream_start();
    }
}

// Stop A2DP streaming
void bt_audio_stop_streaming(void) {
    if (bt_audio_streaming) {
        printf("BT: Stopping A2DP streaming\n");
        bt_audio_streaming = false;
        bt_audio_buffer_pos = 0;
        a2dp_source_stream_stop();
    }
}

// Start Bluetooth device scanning
bool bt_audio_scan_start(void) {
    if (!bt_config.enabled) {
        printf("BT: Bluetooth not initialized\n");
        return false;
    }

    if (scanning) {
        printf("BT: Already scanning\n");
        return false;
    }

    printf("BT: Starting device scan...\n");
    scanning = true;
    gap_inquiry_start(10);  // 10 seconds
    return true;
}

// Stop Bluetooth device scanning
bool bt_audio_scan_stop(void) {
    if (!scanning) {
        return false;
    }

    printf("BT: Stopping device scan\n");
    gap_inquiry_stop();
    scanning = false;
    return true;
}

// Pair with a Bluetooth device
bool bt_audio_pair_device(const char *address) {
    if (!bt_config.enabled) {
        printf("BT: Bluetooth not initialized\n");
        return false;
    }

    printf("BT: Pairing with device %s\n", address);
    
    // Parse MAC address
    bd_addr_t addr;
    if (sscanf(address, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
               &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]) != 6) {
        printf("BT: Invalid MAC address format\n");
        return false;
    }

    pairing_mode = true;
    sm_request_pairing(addr);
    return true;
}

// Unpair a Bluetooth device
bool bt_audio_unpair_device(const char *address) {
    if (!bt_config.enabled) {
        printf("BT: Bluetooth not initialized\n");
        return false;
    }

    printf("BT: Unpairing device %s\n", address);
    
    // Parse MAC address
    bd_addr_t addr;
    if (sscanf(address, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
               &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]) != 6) {
        printf("BT: Invalid MAC address format\n");
        return false;
    }

    sm_delete_bonding(addr);
    return true;
}

// Connect to a Bluetooth device
bool bt_audio_connect_device(const char *address) {
    if (!bt_config.enabled) {
        printf("BT: Bluetooth not initialized\n");
        return false;
    }

    printf("BT: Connecting to device %s\n", address);
    
    // Parse MAC address
    bd_addr_t addr;
    if (sscanf(address, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
               &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]) != 6) {
        printf("BT: Invalid MAC address format\n");
        return false;
    }

    memcpy(connected_addr, addr, 6);
    strcpy(bt_config.connected_device.address, address);
    bt_config.state = BT_AUDIO_CONNECTING;
    
    a2dp_source_establish_stream(addr);
    return true;
}

// Disconnect from current Bluetooth device
bool bt_audio_disconnect_device(void) {
    if (!bt_audio_is_connected()) {
        return false;
    }

    printf("BT: Disconnecting from device\n");
    a2dp_source_disconnect(connected_addr);
    return true;
}

// Get list of paired devices
int bt_audio_get_paired_devices(bt_device_t *devices, int max_devices) {
    // This would need to be implemented with the link key database
    // For now, return 0 (no paired devices)
    return 0;
}

// Process commands from pgusinit
void bt_audio_process_command(uint8_t cmd, const uint8_t *data, uint16_t length) {
    switch (cmd) {
        case CMD_BT_INIT:
            bt_audio_init();
            break;
            
        case CMD_BT_SCAN:
            bt_audio_scan_start();
            break;
            
        case CMD_BT_STOP:
            bt_audio_scan_stop();
            break;
            
        case CMD_BT_PAIR:
            if (data && length > 0) {
                char address[18];
                strncpy(address, (char*)data, 17);
                address[17] = '\0';
                bt_audio_pair_device(address);
            }
            break;
            
        case CMD_BT_UNPAIR:
            if (data && length > 0) {
                char address[18];
                strncpy(address, (char*)data, 17);
                address[17] = '\0';
                bt_audio_unpair_device(address);
            }
            break;
            
        case CMD_BT_CONNECT:
            if (data && length > 0) {
                char address[18];
                strncpy(address, (char*)data, 17);
                address[17] = '\0';
                bt_audio_connect_device(address);
            }
            break;
            
        case CMD_BT_DISCONN:
            bt_audio_disconnect_device();
            break;
            
        case CMD_BT_STATUS:
            // Status is returned via the data port
            break;
            
        case CMD_BT_DEVICES:
            // Device list is returned via the data port
            break;
            
        case CMD_BT_VOLUME:
            if (data && length > 0) {
                bt_audio_set_volume(data[0]);
            }
            break;
            
        default:
            break;
    }
}

#else
// Stub implementations for non-PicoW builds
void bt_audio_init(void) {}
void bt_audio_deinit(void) {}
bool bt_audio_is_connected(void) { return false; }
bt_audio_state_t bt_audio_get_state(void) { return BT_AUDIO_DISCONNECTED; }
void bt_audio_set_volume(uint8_t volume) { (void)volume; }
uint8_t bt_audio_get_volume(void) { return 0; }
void bt_audio_process_audio(int16_t *samples, uint32_t sample_count, uint32_t sample_rate) { 
    (void)samples; (void)sample_count; (void)sample_rate; 
}
void bt_audio_start_streaming(void) {}
void bt_audio_stop_streaming(void) {}
bool bt_audio_scan_start(void) { return false; }
bool bt_audio_scan_stop(void) { return false; }
bool bt_audio_pair_device(const char *address) { (void)address; return false; }
bool bt_audio_unpair_device(const char *address) { (void)address; return false; }
bool bt_audio_connect_device(const char *address) { (void)address; return false; }
bool bt_audio_disconnect_device(void) { return false; }
int bt_audio_get_paired_devices(bt_device_t *devices, int max_devices) { 
    (void)devices; (void)max_devices; return 0; 
}
void bt_audio_process_command(uint8_t cmd, const uint8_t *data, uint16_t length) { 
    (void)cmd; (void)data; (void)length; 
}
#endif