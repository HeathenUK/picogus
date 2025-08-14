//
// btstack_config.h - Configuration for real Bluetooth functionality
//
#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Enable Classic Bluetooth features
#define ENABLE_CLASSIC 1
#define ENABLE_LOG_ERROR 1
#define ENABLE_LOG_INFO 1
#define ENABLE_PRINTF_HEXDUMP 0
#define ENABLE_SCO_OVER_HCI 0
#define ENABLE_SDP_DES_DUMP 0
#define ENABLE_SOFTWARE_AES128 0

// Enable A2DP and AVRCP for audio streaming
#define ENABLE_A2DP_SOURCE 1
#define ENABLE_AVDTP_SOURCE 1
#define ENABLE_AVRCP 1

// Enable GAP for device discovery
#define ENABLE_GAP_INQUIRY 1

// Enable Security Manager for pairing
#define ENABLE_SM 1

// Buffer sizes for audio streaming
#define HCI_ACL_PAYLOAD_SIZE 1024
#define HCI_INCOMING_PRE_BUFFER_SIZE 14
#define MAX_NR_AVDTP_CONNECTIONS 1
#define MAX_NR_AVRCP_CONNECTIONS 1
#define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES 10
#define MAX_NR_GATT_CLIENTS 1
#define MAX_NR_HCI_CONNECTIONS 1
#define MAX_NR_HFP_CONNECTIONS 1
#define MAX_NR_L2CAP_CHANNELS 4
#define MAX_NR_L2CAP_SERVICES 4
#define MAX_NR_RFCOMM_CHANNELS 2
#define MAX_NR_RFCOMM_MULTIPLEXERS 1
#define MAX_NR_RFCOMM_SERVICES 2
#define MAX_NR_SERVICE_RECORD_ITEMS 4
#define MAX_NR_SM_LOOKUP_ENTRIES 10
#define MAX_NR_WHITELIST_ENTRIES 1
#define NVM_NUM_LINK_KEYS 10

// Audio configuration
#define MAX_NR_A2DP_SOURCES 1
#define MAX_NR_A2DP_SINKS 0
#define MAX_NR_AVDTP_SOURCES 1
#define MAX_NR_AVDTP_SINKS 0

// Disable WiFi/lwIP to save memory
#define DISABLE_WIFI 1
#define DISABLE_LWIP 1

#endif // BTSTACK_CONFIG_H