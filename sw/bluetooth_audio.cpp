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
#include <cstdio>
#include <cstring>

#ifdef PICOW

#include "btstack.h"
#include "btstack_config.h"

// Bluetooth audio configuration
static bt_audio_config_t bt_config = {
    .volume = 50,
    .enabled = false,
    .state = BT_AUDIO_DISCONNECTED,
    .connected_device = {0}
};

// BTstack state management
static bool btstack_initialized = false;
static bool inquiry_active = false;
static uint16_t a2dp_cid = 0;
static uint16_t avrcp_cid = 0;
static uint8_t local_seid = 0;
static uint8_t remote_seid = 0;
static bool stream_opened = false;
static bool streaming = false;

// Device discovery
#define MAX_DISCOVERED_DEVICES 20
static bt_device_t discovered_devices[MAX_DISCOVERED_DEVICES];
static int discovered_device_count = 0;

// Audio streaming
static int16_t audio_buffer[1024];  // 2 seconds at 44.1kHz stereo
static uint32_t audio_buffer_pos = 0;
static uint32_t rtp_timestamp = 0;

// SBC encoder for A2DP
static const btstack_sbc_encoder_t *sbc_encoder_instance;
static btstack_sbc_encoder_bluedroid_t sbc_encoder_state;
static uint8_t sbc_storage[1030];
static uint16_t sbc_storage_count = 0;
static bool sbc_ready_to_send = false;

// Audio timer for streaming
static btstack_timer_source_t audio_timer;

// Callback registration
static btstack_packet_callback_registration_t hci_event_callback_registration;

// Forward declarations
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void audio_timer_handler(btstack_timer_source_t *ts);
static void start_audio_streaming(void);
static void stop_audio_streaming(void);

// Initialize BTstack and Bluetooth audio system
void bt_audio_init(void) {
    if (btstack_initialized) {
        printf("BT: Already initialized\n");
        return;
    }

    printf("BT: Initializing BTstack...\n");
    
    // Initialize BTstack
    l2cap_init();
    sdp_init();
    avdtp_source_init();
    avrcp_init();
    gap_init();
    sm_init();
    
    // Register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    
    // Initialize SBC encoder
    sbc_encoder_instance = btstack_sbc_encoder_bluedroid_init_instance();
    btstack_sbc_encoder_bluedroid_init(&sbc_encoder_state, sbc_encoder_instance);
    
    // Set up audio timer
    audio_timer.process = &audio_timer_handler;
    
    btstack_initialized = true;
    bt_config.enabled = true;
    bt_config.state = BT_AUDIO_DISCONNECTED;
    
    printf("BT: BTstack initialized successfully\n");
}

// Deinitialize Bluetooth audio system
void bt_audio_deinit(void) {
    if (!btstack_initialized) {
        return;
    }

    printf("BT: Deinitializing Bluetooth audio...\n");
    
    if (bt_audio_is_connected()) {
        bt_audio_disconnect_device();
    }
    
    if (inquiry_active) {
        gap_inquiry_stop();
        inquiry_active = false;
    }
    
    btstack_initialized = false;
    bt_config.enabled = false;
    bt_config.state = BT_AUDIO_DISCONNECTED;
    
    printf("BT: Bluetooth audio deinitialized\n");
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

    // Buffer audio for A2DP streaming
    if (streaming && bt_config.state == BT_AUDIO_STREAMING) {
        uint32_t samples_to_copy = sample_count * 2;  // Stereo samples
        if (audio_buffer_pos + samples_to_copy <= sizeof(audio_buffer) / sizeof(int16_t)) {
            memcpy(&audio_buffer[audio_buffer_pos], samples, samples_to_copy * sizeof(int16_t));
            audio_buffer_pos += samples_to_copy;
        }
    }
}

// Start A2DP streaming
void bt_audio_start_streaming(void) {
    if (bt_config.enabled && bt_config.state == BT_AUDIO_CONNECTED && stream_opened) {
        printf("BT: Starting A2DP streaming\n");
        start_audio_streaming();
    }
}

// Stop A2DP streaming
void bt_audio_stop_streaming(void) {
    if (streaming) {
        printf("BT: Stopping A2DP streaming\n");
        stop_audio_streaming();
    }
}

// Start Bluetooth device scanning
bool bt_audio_scan_start(void) {
    if (!btstack_initialized) {
        printf("BT: Bluetooth not initialized\n");
        return false;
    }

    if (inquiry_active) {
        printf("BT: Scan already active\n");
        return true;
    }

    printf("BT: Starting 10-second device scan...\n");
    
    // Clear previous discoveries
    discovered_device_count = 0;
    memset(discovered_devices, 0, sizeof(discovered_devices));
    
    // Start inquiry for 10 seconds (8 * 1.28s ≈ 10.24s)
    gap_inquiry_start(8);
    inquiry_active = true;
    
    return true;
}

// Stop Bluetooth device scanning
bool bt_audio_scan_stop(void) {
    if (!inquiry_active) {
        return true;
    }

    printf("BT: Stopping device scan\n");
    gap_inquiry_stop();
    inquiry_active = false;
    return true;
}

// Pair with a Bluetooth device
bool bt_audio_pair_device(const char *address) {
    if (!btstack_initialized) {
        printf("BT: Bluetooth not initialized\n");
        return false;
    }

    printf("BT: Pairing with device %s\n", address);
    
    bd_addr_t addr;
    sscanf_bd_addr(address, addr);
    
    // Start pairing process
    sm_request_pairing(addr);
    
    return true;
}

// Unpair a Bluetooth device
bool bt_audio_unpair_device(const char *address) {
    if (!btstack_initialized) {
        printf("BT: Bluetooth not initialized\n");
        return false;
    }

    printf("BT: Unpairing device %s\n", address);
    
    bd_addr_t addr;
    sscanf_bd_addr(address, addr);
    
    // Remove link key
    sm_delete_link_key(addr);
    
    // Clear from connected device if it matches
    if (strcmp(bt_config.connected_device.address, address) == 0) {
        memset(&bt_config.connected_device, 0, sizeof(bt_device_t));
    }
    
    return true;
}

// Connect to a Bluetooth device
bool bt_audio_connect_device(const char *address) {
    if (!btstack_initialized) {
        printf("BT: Bluetooth not initialized\n");
        return false;
    }

    printf("BT: Connecting to device %s\n", address);
    
    bd_addr_t addr;
    sscanf_bd_addr(address, addr);
    
    // Store device info
    strcpy(bt_config.connected_device.address, address);
    bt_config.connected_device.connected = false;
    bt_config.state = BT_AUDIO_CONNECTING;
    
    // Start A2DP connection
    a2dp_source_establish_stream(addr, &a2dp_cid);
    
    return true;
}

// Disconnect from current Bluetooth device
bool bt_audio_disconnect_device(void) {
    if (!bt_audio_is_connected()) {
        return false;
    }

    printf("BT: Disconnecting from device\n");
    
    if (streaming) {
        stop_audio_streaming();
    }
    
    if (a2dp_cid) {
        a2dp_source_disconnect(a2dp_cid);
        a2dp_cid = 0;
    }
    
    if (avrcp_cid) {
        avrcp_disconnect(avrcp_cid);
        avrcp_cid = 0;
    }
    
    bt_config.state = BT_AUDIO_DISCONNECTED;
    bt_config.connected_device.connected = false;
    stream_opened = false;
    
    return true;
}

// Get list of paired devices
int bt_audio_get_paired_devices(bt_device_t *devices, int max_devices) {
    if (!btstack_initialized) {
        return 0;
    }

    // For now, return the connected device if it exists
    int count = 0;
    if (bt_config.connected_device.paired && count < max_devices) {
        devices[count] = bt_config.connected_device;
        count++;
    }
    
    return count;
}

// Get list of discovered devices from last scan
int bt_audio_get_discovered_devices(bt_device_t *devices, int max_devices) {
    if (!btstack_initialized) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < discovered_device_count && count < max_devices; i++) {
        devices[count] = discovered_devices[i];
        count++;
    }
    
    return count;
}

// Get number of discovered devices
int bt_audio_get_discovered_device_count(void) {
    return discovered_device_count;
}

// Check if scanning is currently active
bool bt_audio_is_scanning(void) {
    return inquiry_active;
}

// Audio timer handler for streaming
static void audio_timer_handler(btstack_timer_source_t *ts) {
    if (!streaming || !stream_opened) {
        return;
    }
    
    // Process audio buffer and send via A2DP
    if (audio_buffer_pos > 0 && sbc_ready_to_send) {
        // Encode audio data with SBC
        int bytes_encoded = btstack_sbc_encoder_bluedroid_encode_signed_16(&sbc_encoder_state, 
                                                                          audio_buffer, 
                                                                          audio_buffer_pos / 2,  // Convert to mono samples
                                                                          sbc_storage, 
                                                                          sizeof(sbc_storage));
        
        if (bytes_encoded > 0) {
            // Send encoded audio data
            avdtp_source_stream_send_media_packet(a2dp_cid, local_seid, 
                                                 sbc_storage, bytes_encoded, 
                                                 rtp_timestamp);
            rtp_timestamp += audio_buffer_pos / 2;  // Increment timestamp
        }
        
        // Reset buffer
        audio_buffer_pos = 0;
    }
    
    // Schedule next audio timer
    btstack_run_loop_set_timer(ts, 10);  // 10ms intervals
    btstack_run_loop_add_timer(ts);
}

// Start audio streaming
static void start_audio_streaming(void) {
    if (!stream_opened) {
        return;
    }
    
    streaming = true;
    audio_buffer_pos = 0;
    rtp_timestamp = 0;
    bt_config.state = BT_AUDIO_STREAMING;
    
    // Start audio timer
    btstack_run_loop_set_timer(&audio_timer, 10);
    btstack_run_loop_add_timer(&audio_timer);
    
    printf("BT: Audio streaming started\n");
}

// Stop audio streaming
static void stop_audio_streaming(void) {
    streaming = false;
    btstack_run_loop_remove_timer(&audio_timer);
    
    if (bt_config.state == BT_AUDIO_STREAMING) {
        bt_config.state = BT_AUDIO_CONNECTED;
    }
    
    printf("BT: Audio streaming stopped\n");
}

// Main packet handler for all Bluetooth events
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);
    
    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }
    
    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("BT: Disconnected\n");
            bt_config.state = BT_AUDIO_DISCONNECTED;
            bt_config.connected_device.connected = false;
            stream_opened = false;
            streaming = false;
            break;
            
        case GAP_EVENT_INQUIRY_RESULT:
            if (discovered_device_count < MAX_DISCOVERED_DEVICES) {
                gap_event_inquiry_result_get_bd_addr(packet, discovered_devices[discovered_device_count].address);
                gap_event_inquiry_result_get_name(packet, discovered_devices[discovered_device_count].name);
                discovered_devices[discovered_device_count].paired = false;
                discovered_devices[discovered_device_count].connected = false;
                discovered_device_count++;
                printf("BT: Found device %s (%s)\n", 
                       discovered_devices[discovered_device_count-1].address,
                       discovered_devices[discovered_device_count-1].name);
            }
            break;
            
        case GAP_EVENT_INQUIRY_COMPLETE:
            printf("BT: Device scan completed, found %d devices:\n", discovered_device_count);
            for (int i = 0; i < discovered_device_count; i++) {
                printf("BT:   %d. %s (%s)\n", i + 1, 
                       discovered_devices[i].address,
                       discovered_devices[i].name[0] ? discovered_devices[i].name : "Unknown");
            }
            inquiry_active = false;
            break;
            
        case AVDTP_EVENT_SIGNALING_CONNECTION_ESTABLISHED:
            printf("BT: AVDTP connection established\n");
            a2dp_cid = avdtp_event_signaling_connection_established_get_avdtp_cid(packet);
            break;
            
        case AVDTP_EVENT_SIGNALING_CONNECTION_RELEASED:
            printf("BT: AVDTP connection released\n");
            a2dp_cid = 0;
            stream_opened = false;
            break;
            
        case AVDTP_EVENT_STREAM_ESTABLISHED:
            printf("BT: A2DP stream established\n");
            local_seid = avdtp_event_stream_established_get_local_seid(packet);
            remote_seid = avdtp_event_stream_established_get_remote_seid(packet);
            stream_opened = true;
            bt_config.state = BT_AUDIO_CONNECTED;
            bt_config.connected_device.connected = true;
            bt_config.connected_device.paired = true;
            sbc_ready_to_send = true;
            break;
            
        case AVDTP_EVENT_STREAM_RELEASED:
            printf("BT: A2DP stream released\n");
            stream_opened = false;
            streaming = false;
            sbc_ready_to_send = false;
            break;
            
        case AVRCP_EVENT_CONNECTION_ESTABLISHED:
            printf("BT: AVRCP connection established\n");
            avrcp_cid = avrcp_event_connection_established_get_avrcp_cid(packet);
            break;
            
        case AVRCP_EVENT_CONNECTION_RELEASED:
            printf("BT: AVRCP connection released\n");
            avrcp_cid = 0;
            break;
            
        case SM_EVENT_PAIRING_COMPLETE:
            if (sm_event_pairing_complete_get_status(packet) == 0) {
                printf("BT: Pairing successful\n");
                bt_config.connected_device.paired = true;
            } else {
                printf("BT: Pairing failed\n");
            }
            break;
            
        default:
            break;
    }
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
            // Return discovered devices count and list
            printf("BT: Discovered devices: %d\n", bt_audio_get_discovered_device_count());
            if (bt_audio_get_discovered_device_count() > 0) {
                bt_device_t devices[MAX_DISCOVERED_DEVICES];
                int count = bt_audio_get_discovered_devices(devices, MAX_DISCOVERED_DEVICES);
                for (int i = 0; i < count; i++) {
                    printf("BT: Device %d: %s (%s)\n", i + 1, 
                           devices[i].address,
                           devices[i].name[0] ? devices[i].name : "Unknown");
                }
            }
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
int bt_audio_get_discovered_devices(bt_device_t *devices, int max_devices) { 
    (void)devices; (void)max_devices; return 0; 
}
int bt_audio_get_discovered_device_count(void) { return 0; }
bool bt_audio_is_scanning(void) { return false; }
void bt_audio_process_command(uint8_t cmd, const uint8_t *data, uint16_t length) { 
    (void)cmd; (void)data; (void)length; 
}
#endif