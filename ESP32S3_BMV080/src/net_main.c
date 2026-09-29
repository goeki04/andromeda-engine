/**
 * @file net_main.c
 * @brief Wi-Fi connection and transport of the sensor data
 *
 * Two modes, selected by NET_MODE_ACCESS_POINT in net_main.h:
 *
 * Access point mode: the board opens its own Wi-Fi and serves a small web page that shows
 * the incoming lines live. The lines are kept in a ring buffer, the browser asks for
 * everything newer than the last line it has seen.
 *
 * Client mode: the board joins an existing access point and sends the same lines
 * as UDP datagrams to the gateway of that network.
 *
 * In both cases the format of the lines is the one that was used for the BLE notifications:
 *     PFF,uptime,channel,frequency,snr\n
 *     {"topic":"bmv080", ...}\n
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/sockets.h"

#include "net_main.h"
#include "diag.h"

#if NET_MODE_ACCESS_POINT
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_timer.h"
#endif

/** @brief Unique part of the board id, created in main.c */
extern char shortId[7];

/** @brief True as soon as data can be handed over */
static volatile bool is_ready = false;

#if NET_MODE_ACCESS_POINT

/*********************************************************************************************************************
 * Ring buffer of the received lines
 *********************************************************************************************************************/

/** @brief Buffered lines, the line with number n is stored at index n % NET_BUFFER_LINES */
static char lines[NET_BUFFER_LINES][NET_LINE_LENGTH];
/** @brief Number the next line gets */
static volatile uint32_t next_sequence = 0;
/** @brief Protects the buffer between the sensor task and the web server task */
static SemaphoreHandle_t lines_mutex = NULL;

/**
 * @brief Stores one line in the ring buffer
 */
static void store_line(const char *line, size_t length)
{
  while ((length > 0) && ((*line == '\r') || (*line == ' ')))
  {
    line++;
    length--;
  }
  while ((length > 0) && ((line[length - 1] == '\r') || (line[length - 1] == '\n')))
  {
    length--;
  }
  /* only the particle feature frames are shown, the longer JSON lines of the
     particulate matter and BME690 values would not fit into one buffer line */
  if ((length < 4) || (memcmp(line, "PFF,", 4) != 0))
  {
    return;
  }
  if (length > (NET_LINE_LENGTH - 1))
  {
    length = NET_LINE_LENGTH - 1;
  }

  char *slot = lines[next_sequence % NET_BUFFER_LINES];
  memcpy(slot, line, length);
  slot[length] = '\0';

  next_sequence++;
}

/**
 * @brief Splits a buffer into lines and stores them
 */
static void store_buffer(const char *data, size_t length)
{
  if (lines_mutex == NULL)
  {
    return;
  }

  xSemaphoreTake(lines_mutex, portMAX_DELAY);

  size_t start = 0;
  for (size_t i = 0; i < length; i++)
  {
    if (data[i] == '\n')
    {
      store_line(&data[start], i - start);
      start = i + 1;
    }
  }
  if (start < length)
  {
    store_line(&data[start], length - start);
  }

  xSemaphoreGive(lines_mutex);
}

/*********************************************************************************************************************
 * Web page and web server
 *********************************************************************************************************************/

/** @brief Buffer for one answer of the /pff endpoint */
static char reply_buffer[24 * 1024];

/** @brief The web page, generated from web/index.html into src/web_page.c */
extern const uint8_t web_page_html[];
extern const size_t web_page_html_length;

/**
 * @brief Delivers the web page
 */
static esp_err_t root_get_handler(httpd_req_t *req)
{
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)web_page_html, (ssize_t)web_page_html_length);
}

/**
 * @brief Delivers all lines newer than the requested number
 * @details First line of the answer is "<next number> <0 or 1 for lost lines>",
 *          after that one line per entry.
 */
static esp_err_t pff_get_handler(httpd_req_t *req)
{
  uint32_t last   = next_sequence;
  uint32_t oldest = (last > NET_BUFFER_LINES) ? (last - NET_BUFFER_LINES) : 0;
  uint32_t since  = (last > 50) ? (last - 50) : 0;   /* first request: the last 50 lines */
  bool lost = false;

  char query[64];
  char value[16];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
  {
    if (httpd_query_key_value(query, "since", value, sizeof(value)) == ESP_OK)
    {
      since = (uint32_t)strtoul(value, NULL, 10);
    }
  }

  if (since < oldest)
  {
    since = oldest;
    lost = true;
  }
  if (since > last)
  {
    since = last;   /* board was restarted */
  }
  if ((last - since) > NET_MAX_REPLY_LINES)
  {
    since = last - NET_MAX_REPLY_LINES;
    lost = true;
  }

  int used = snprintf(reply_buffer, sizeof(reply_buffer), "%lu %d\n", (unsigned long)last, lost ? 1 : 0);

  xSemaphoreTake(lines_mutex, portMAX_DELAY);
  for (uint32_t sequence = since; sequence < last; sequence++)
  {
    int written = snprintf(&reply_buffer[used], sizeof(reply_buffer) - used, "%s\n", lines[sequence % NET_BUFFER_LINES]);
    if ((written <= 0) || ((used + written) >= (int)sizeof(reply_buffer)))
    {
      break;
    }
    used += written;
  }
  xSemaphoreGive(lines_mutex);

  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, reply_buffer, used);
}

/** @brief Number of frames the sensor task had to throw away, from bmv080_main.c */
extern uint32_t bmv080_get_dropped_count(void);

/**
 * @brief Delivers the counters and the kept log lines
 * @details The board normally runs without a USB cable, this replaces the serial monitor.
 */
static esp_err_t status_get_handler(httpd_req_t *req)
{
  int used = snprintf(reply_buffer, sizeof(reply_buffer),
                      "uptime      : %llu s\n"
                      "free heap   : %lu bytes\n"
                      "frames total: %lu\n"
                      "frames lost : %lu\n"
                      "\n",
                      (unsigned long long)(esp_timer_get_time() / 1000000),
                      (unsigned long)esp_get_free_heap_size(),
                      (unsigned long)next_sequence,
                      (unsigned long)bmv080_get_dropped_count());

  if ((used > 0) && (used < (int)sizeof(reply_buffer)))
  {
    used += (int)diag_dump(&reply_buffer[used], sizeof(reply_buffer) - (size_t)used);
  }

  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, reply_buffer, used);
}

/**
 * @brief Starts the web server with both endpoints
 */
static void start_web_server(void)
{
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;

  if (httpd_start(&server, &config) != ESP_OK)
  {
    diag_log("Starting the web server failed");
    return;
  }

  httpd_uri_t root_uri = { .uri = "/",    .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL };
  httpd_uri_t pff_uri  = { .uri = "/pff", .method = HTTP_GET, .handler = pff_get_handler,  .user_ctx = NULL };
  httpd_uri_t stat_uri = { .uri = "/status", .method = HTTP_GET, .handler = status_get_handler, .user_ctx = NULL };

  httpd_register_uri_handler(server, &root_uri);
  httpd_register_uri_handler(server, &pff_uri);
  httpd_register_uri_handler(server, &stat_uri);
}

/**
 * @brief Opens the own access point
 */
static void start_access_point(void)
{
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&init_config));

  wifi_config_t wifi_config = {
    .ap = {
      .channel        = 1,
      .max_connection = 4,
      .authmode       = WIFI_AUTH_WPA2_PSK,
    },
  };

  int ssid_length = snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid),
                             "%s", NET_AP_SSID);
  wifi_config.ap.ssid_len = (uint8_t)ssid_length;
  strncpy((char *)wifi_config.ap.password, NET_AP_PASSWORD, sizeof(wifi_config.ap.password) - 1);

  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

  start_web_server();
  is_ready = true;

  diag_log("Access point \"%s\" started, password \"%s\", web page on http://192.168.4.1",
         (char *)wifi_config.ap.ssid, NET_AP_PASSWORD);
}

#else /* client mode */

/*********************************************************************************************************************
 * UDP transport
 *********************************************************************************************************************/

/** @brief Socket used for all outgoing datagrams */
static int udp_socket = -1;
/** @brief Address the datagrams are sent to */
static struct sockaddr_in target_address;

/**
 * @brief Handles the Wi-Fi events of the station interface
 * @details Connects on start and reconnects after a disconnect, so the board also comes up
 *          when the access point is switched on later.
 */
static void on_wifi_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  if (event_id == WIFI_EVENT_STA_START)
  {
    esp_wifi_connect();
  }
  else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    is_ready = false;
    esp_wifi_connect();
  }
}

/**
 * @brief Stores the target address once the board received its IP address
 * @note  Without an explicit NET_TARGET_IP the gateway is used, which is the access point itself.
 */
static void on_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port   = htons(NET_UDP_PORT);

  if (strlen(NET_TARGET_IP) > 0)
  {
    target_address.sin_addr.s_addr = inet_addr(NET_TARGET_IP);
  }
  else
  {
    target_address.sin_addr.s_addr = event->ip_info.gw.addr;
  }

  is_ready = true;

  diag_log("Wi-Fi connected, own address " IPSTR ", sending to " IPSTR ":%d\r\n",
         IP2STR(&event->ip_info.ip), IP2STR(&event->ip_info.gw), NET_UDP_PORT);
}

/**
 * @brief Joins the configured access point
 */
static void start_client(void)
{
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&init_config));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL, NULL));

  wifi_config_t wifi_config = {
    .sta = {
      .ssid               = NET_WIFI_SSID,
      .password           = NET_WIFI_PASSWORD,
      .threshold.authmode = WIFI_AUTH_WPA2_PSK,
    },
  };

  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  /* no power save, otherwise the frames are delayed until the next beacon */
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

  udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (udp_socket < 0)
  {
    diag_log("Creating the UDP socket failed with errno %d", errno);
  }

  diag_log("Connecting to Wi-Fi %s ...", NET_WIFI_SSID);
}

#endif /* NET_MODE_ACCESS_POINT */

/*********************************************************************************************************************
 * Interface used by the sensor tasks
 *********************************************************************************************************************/

void net_init(void)
{
#if NET_MODE_ACCESS_POINT
  lines_mutex = xSemaphoreCreateMutex();
  start_access_point();
#else
  start_client();
#endif
}

uint16_t net_get_payload_size(void)
{
  if (!is_ready)
  {
    return 0;
  }
  return NET_PAYLOAD_SIZE;
}

bool net_send_bytes(const uint8_t *data, uint16_t length)
{
  if ((!is_ready) || (length == 0))
  {
    return false;
  }

#if NET_MODE_ACCESS_POINT
  store_buffer((const char *)data, length);
  return true;
#else
  if (udp_socket < 0)
  {
    return false;
  }
  int sent = sendto(udp_socket, data, length, 0, (struct sockaddr *)&target_address, sizeof(target_address));
  return (sent == (int)length);
#endif
}

void net_send_message(const char *message)
{
  if (message == NULL)
  {
    return;
  }

  size_t length = strlen(message);
  if (length > NET_PAYLOAD_SIZE)
  {
    length = NET_PAYLOAD_SIZE;
  }

  (void)net_send_bytes((const uint8_t *)message, (uint16_t)length);
}
