//
// btstack_config.h for PicoW A2DP audio streaming (Bluetooth only, no WiFi)
//
// Documentation: https://bluekitchen-gmbh.com/btstack/#how_to/
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features
#define HAVE_ASSERT
#define HAVE_MALLOC
#define HAVE_POSIX_TIME

// Disable WiFi/lwIP features
#define DISABLE_WIFI
#define DISABLE_LWIP

// BTstack features that can be enabled
#define ENABLE_CLASSIC
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
#define ENABLE_PRINTF_HEXDUMP
#define ENABLE_SCO_OVER_HCI
#define ENABLE_SDP_DES_DUMP
#define ENABLE_SOFTWARE_AES128

// A2DP Source features
#define ENABLE_A2DP_SOURCE
#define ENABLE_AVDTP_SOURCE
#define ENABLE_AVRCP
#define ENABLE_GAP_INQUIRY
#define ENABLE_SM

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE (1691 + 4)
#define HCI_INCOMING_PRE_BUFFER_SIZE 14 // sizeof benep heade, avoid memcpy

#define NVM_NUM_DEVICE_DB_ENTRIES      16
#define NVM_NUM_LINK_KEYS              16

// A2DP configuration
#define A2DP_SOURCE_NUM_CONNECTIONS    1
#define A2DP_SOURCE_MAX_NUM_SEID      4
#define A2DP_SOURCE_MAX_NUM_MEDIA_CODECS 4

// SBC codec configuration
#define ENABLE_SBC_FRAMEWORK
#define SBC_FRAMEWORK_MAX_NUM_FRAMES_PER_PACKET 1

// Memory configuration
#define MAX_NR_AVDTP_CONNECTIONS       1
#define MAX_NR_AVDTP_STREAM_ENDPOINTS 4
#define MAX_NR_AVRCP_CONNECTIONS       1
#define MAX_NR_GATT_CLIENTS            0
#define MAX_NR_HCI_CONNECTIONS         1
#define MAX_NR_L2CAP_CHANNELS          4
#define MAX_NR_L2CAP_SERVICES          2
#define MAX_NR_RFCOMM_CHANNELS         0
#define MAX_NR_RFCOMM_MULTIPLEXERS     0
#define MAX_NR_RFCOMM_SERVICES         0
#define MAX_NR_SERVICE_RECORD_ITEMS    1
#define MAX_NR_SM_LOOKUP_ENTRIES       3
#define MAX_NR_WHITELIST_ENTRIES       1

// HCI configuration
#define MAX_NR_HCI_CONNECTIONS         1
#define MAX_NR_WHITELIST_ENTRIES       1

// L2CAP configuration
#define MAX_NR_L2CAP_CHANNELS          4
#define MAX_NR_L2CAP_SERVICES          2

// Security Manager configuration
#define MAX_NR_SM_LOOKUP_ENTRIES       3

// SDP configuration
#define MAX_NR_SERVICE_RECORD_ITEMS    1

#endif