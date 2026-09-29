#ifndef NET_MAIN_H
#define NET_MAIN_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Operating mode of the Wi-Fi interface
 *  @details 1: the board opens its own access point and serves the web page itself,
 *              a laptop or phone connects to it and opens http://192.168.4.1
 *           0: the board joins an existing access point and sends the data
 *              as UDP datagrams to the gateway of that network */
#define NET_MODE_ACCESS_POINT 1

/** @brief Name of the own access point */
#define NET_AP_SSID           "BMV080_SENSOR"
/** @brief Password of the own access point, at least 8 characters */
#define NET_AP_PASSWORD       "12345678"

/** @brief Access point the board joins in client mode */
#define NET_WIFI_SSID         "BMV080_SENSOR"
/** @brief Password of that access point */
#define NET_WIFI_PASSWORD     "12345678"

/** @brief UDP port the receiver listens on in client mode */
#define NET_UDP_PORT          4210

/** @brief Target address of the data, an empty string sends to the access point itself
 *  @note  The access point is the gateway of the network, therefore the same firmware also works
 *         with a laptop hotspot, the data then arrives at the laptop. */
#define NET_TARGET_IP         ""

/** @brief Usable payload of one UDP datagram in bytes, stays below the ethernet MTU of 1500 */
#define NET_PAYLOAD_SIZE      1400

/** @brief Number of lines the web page can look back, about two seconds at full rate */
#define NET_BUFFER_LINES      512
/** @brief Maximum length of one buffered line */
#define NET_LINE_LENGTH       72
/** @brief Maximum number of lines answered per request */
#define NET_MAX_REPLY_LINES   400

/** @brief Starts the access point with the web server, or the client with the UDP socket */
void net_init(void);

/** @brief Usable payload size of one transfer in bytes, 0 if the board is not ready */
uint16_t net_get_payload_size(void);

/** @brief Hands over raw bytes, returns false if they could not be handled */
bool net_send_bytes(const uint8_t *data, uint16_t length);

/** @brief Hands over a zero terminated string */
void net_send_message(const char *message);

#endif // NET_MAIN_H
