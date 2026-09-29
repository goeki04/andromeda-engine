
/**
 * @file bmv080_app.c
 * @brief BMV080 sensor application
 *
 * This file contains the implementation of the BMV080 sensor application,
 * including initialization, data reading, and MQTT publishing.
 */
 
 
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <stdio.h>
#include <string.h>
#include "peripherals.h"
#include "bmv080.h"
#include "bmv080_hal_engineering.h"
#include "bmv080_io.h"
#include "driver/temperature_sensor.h"
#include "net_main.h"
#include "diag.h"

#if BMV080_USE_I2C
/** @brief I2C port number, the sensor driver gets a pointer to it as its sercom handle */
static int hi2c = 0;
#else
/** @brief SPI device handle */
spi_device_handle_t hspi;
#endif
//extern bool isConnected;
//extern esp_mqtt_client_handle_t client;
extern void bmv080_publish(const char *buffer);
extern char shortId[7];
/** @brief Temperature sensor handle */
temperature_sensor_handle_t temp_sensor = NULL;
/** @brief Flag indicating if BMV080 data has been published */
volatile bool flBMV080Published = false;

/** @brief Number of PFF frames the queue can buffer until the network task sends them */
#define PFF_QUEUE_LENGTH      512
/** @brief Size of the buffer collecting PFF lines for one datagram */
#define PFF_NET_BUFFER_SIZE   1400
/** @brief Maximum time in ms a PFF line waits before it is sent */
#define PFF_NET_FLUSH_MS      50

/** @brief Queue between the PFF callback and the network sender task */
static QueueHandle_t pff_queue = NULL;
/** @brief Number of PFF frames dropped because the queue was full */
static volatile uint32_t pff_dropped_count = 0;

/**
 * @brief Callback function for BMV080 data ready event
 * @param bmv080_output BMV080 sensor output data
 * @param callback_parameters Callback parameters (unused)
 */
void bmv080_data_ready(bmv080_output_t bmv080_output, void* callback_parameters)
{
static char buffer[256] = {0};
//  gpio_set_level(B_LED_PIN, 1);
//  gpio_hold_en(B_LED_PIN);

float temperature;
  temperature_sensor_get_celsius(temp_sensor, &temperature);

  snprintf(buffer,256,"{\"topic\":\"bmv080\",\"data\":{\"ID\":\"%s\",\"R\":%.1f,\"PM10\":%.0f,\"PM25\":%.0f,\"PM1\":%.0f,\"obst\":\"%s\",\"omr\":\"%s\",\"T\":%.1f}}\n", shortId,
        bmv080_output.runtime_in_sec, bmv080_output.pm10_mass_concentration, bmv080_output.pm2_5_mass_concentration, bmv080_output.pm1_mass_concentration, 
        (bmv080_output.is_obstructed ? "yes" : "no"), (bmv080_output.is_outside_measurement_range ? "yes" : "no"),
        temperature);     

//  printf(buffer);

  net_send_message(buffer);
  flBMV080Published = true;
//  gpio_set_level(B_LED_PIN, 0);
//  gpio_hold_en(B_LED_PIN);
}

/**
 * @brief Callback function for each particle feature frame (PFF) of the BMV080
 * @param pff_data Particle feature data of one detection
 * @param callback_parameters Callback parameters (unused)
 * @note Called within bmv080_serve_interrupt(), must return quickly, therefore the frame is only queued
 */
void bmv080_pff_ready(const bmv080_pff_data_t* pff_data, void* callback_parameters)
{
  if (xQueueSend(pff_queue, pff_data, 0) != pdTRUE)
  {
    pff_dropped_count++;
  }
}

/**
 * @brief Sends one buffer as UDP datagram(s)
 * @details Normally the buffer contains complete lines and fits into one datagram,
 *          it is only split if the buffer is larger than the usable payload.
 */
static void pff_net_flush(const char* buffer, size_t length)
{
  size_t offset = 0;
  while (offset < length)
  {
    uint16_t payload_size = net_get_payload_size();
    if (payload_size == 0)
    {
      return; /* not connected, data is discarded */
    }
    uint16_t chunk = (uint16_t)(((length - offset) < payload_size) ? (length - offset) : payload_size);
    if (!net_send_bytes((const uint8_t*)&buffer[offset], chunk))
    {
      return;
    }
    offset += chunk;
  }
}

/**
 * @brief Task sending the queued PFF frames over Wi-Fi
 * @details Each frame is sent as text line "PFF,uptime,channel,frequency,snr\n".
 *          Lines are collected until a datagram is full, the queue is empty or PFF_NET_FLUSH_MS elapsed.
 */
static void pff_net_task(void *pvParameter)
{
  static char buffer[PFF_NET_BUFFER_SIZE];
  size_t used = 0;
  uint32_t reported_dropped_count = 0;
  bmv080_pff_data_t pff_data;

  for(;;)
  {
    bool received = (xQueueReceive(pff_queue, &pff_data, pdMS_TO_TICKS(PFF_NET_FLUSH_MS)) == pdTRUE);
    uint16_t payload_size = net_get_payload_size();

    if (payload_size == 0)
    {
      used = 0; /* not connected, discard */
      continue;
    }

    size_t limit = (payload_size < sizeof(buffer)) ? payload_size : sizeof(buffer);

    if (received)
    {
      char line[96];
      int length = snprintf(line, sizeof(line), "PFF,%.3f,%d,%.2f,%.2f\n",
                            pff_data.uptime_in_sec, (int)pff_data.channel_id,
                            pff_data.frequency, pff_data.snr);
      if ((length <= 0) || (length >= (int)sizeof(line)))
      {
        continue;
      }

      /* send collected lines first if the new line does not fit anymore */
      if ((used > 0) && ((used + (size_t)length) > limit))
      {
        pff_net_flush(buffer, used);
        used = 0;
      }

      memcpy(&buffer[used], line, (size_t)length);
      used += (size_t)length;
    }

    /* send when nothing else is waiting, so that single frames are not delayed */
    if ((used > 0) && (!received || (uxQueueMessagesWaiting(pff_queue) == 0)))
    {
      pff_net_flush(buffer, used);
      used = 0;
    }

    if (pff_dropped_count != reported_dropped_count)
    {
      reported_dropped_count = pff_dropped_count;
      diag_log("PFF queue full, %lu frames dropped in total", (unsigned long)reported_dropped_count);
    }
  }
}

/**
 * @brief Number of frames that had to be thrown away, shown on the status page
 */
uint32_t bmv080_get_dropped_count(void)
{
  return pff_dropped_count;
}

/**
 * @brief Get current system time in milliseconds
 * @return Milliseconds since system boot
 * @note Uses FreeRTOS tick count converted to milliseconds
 */
uint32_t get_tick_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/*********************************************************************************************************************
 * PFF measurement settings, identical to the Raspberry Pi example (bmv080_pff_measurement.c)
 *********************************************************************************************************************/

static float pff_snr_threshold          = 1500.0f;
static uint16_t pff_pd_time             = 2441;   /* particle detection duration */
static uint16_t pff_min_noise_exp       = 0x14;   /* minimum noise exponent      */
static uint16_t pff_lagcnt_init         = 10;     /* lag counter init value      */
static int pff_initial_time_seconds     = 10;     /* noise floor settling time   */
static bool pff_enable_high_rate_config = true;

/** @brief Pause between two serve interrupt calls in ms
 *  @note  The Raspberry Pi polls without pause, on the ESP32 the task has to yield to the BLE and idle tasks.
 *         The FreeRTOS tick is 10 ms (CONFIG_FREERTOS_HZ=100), so this is the shortest possible pause. */
#define PFF_POLL_DELAY_MS     10

/**
 * @brief Prints the result of a configuration step
 * @return true if the step was successful
 */
static bool pff_step_ok(const char* step, bmv080_status_code_t status)
{
  if (status != E_BMV080_OK)
  {
    diag_log("%s failed with BMV080 status %d", step, (int)status);
    gpio_set_level(R_LED_PIN, 1);
    gpio_hold_en(R_LED_PIN);
    return false;
  }
  diag_log("%s: OK", step);
  return true;
}

static uint16_t set_field(uint16_t value, unsigned shift, unsigned width, uint16_t field_value)
{
  uint16_t mask = (uint16_t)(((1u << width) - 1u) << shift);
  return (uint16_t)((value & (uint16_t)~mask) | ((uint16_t)(field_value << shift) & mask));
}

/**
 * @brief Enables or stops the noise floor (NA) update of all channels (DP_USER register 0x100, bit 0)
 */
static bmv080_status_code_t set_na_update_stopped(bmv080_handle_t handle, bool stopped)
{
  bmv080_status_code_t status = bmv080_hal_set_mode(handle, E_CHANNEL_ALL, E_BMV080_CONFIG_MODE);
  if (status != E_BMV080_OK) return status;

  for (bmv080_channel_id_t ch = E_CHANNEL_ID_1; ch <= E_CHANNEL_ID_3; ch++)
  {
    bmv080_reg_t reg;
    status = bmv080_hal_get_regs(handle, ch, 0x100, &reg, 1);
    if (status != E_BMV080_OK) return status;

    reg.value = (uint16_t)((reg.value & ~0x0001u) | (stopped ? 1u : 0u));
    status = bmv080_hal_set_regs(handle, ch, 0x100, &reg, 1);
    if (status != E_BMV080_OK) return status;
  }

  return bmv080_hal_set_mode(handle, E_CHANNEL_ALL, E_BMV080_STANDBY_MODE);
}

/**
 * @brief Writes the high rate register configuration of all channels
 */
static bmv080_status_code_t apply_high_rate_config(bmv080_handle_t handle)
{
  bmv080_status_code_t status = bmv080_hal_set_mode(handle, E_CHANNEL_ALL, E_BMV080_CONFIG_MODE);
  if (status != E_BMV080_OK) return status;

  for (bmv080_channel_id_t ch = E_CHANNEL_ID_1; ch <= E_CHANNEL_ID_3; ch++)
  {
    bmv080_reg_t reg;

    /* FIFO_CONFIG (0x0A0) streaming mode */
    status = bmv080_hal_get_regs(handle, ch, 0x0A0, &reg, 1);
    if (status != E_BMV080_OK) return status;
    reg.value = set_field(reg.value, 0, 1, 0);
    status = bmv080_hal_set_regs(handle, ch, 0x0A0, &reg, 1);
    if (status != E_BMV080_OK) return status;

    /* PD_TIME (0x106) */
    status = bmv080_hal_get_regs(handle, ch, 0x106, &reg, 1);
    if (status != E_BMV080_OK) return status;
    reg.value = set_field(reg.value, 0, 13, pff_pd_time);
    status = bmv080_hal_set_regs(handle, ch, 0x106, &reg, 1);
    if (status != E_BMV080_OK) return status;

    /* MIN_NOISE_EXP (0x122) */
    status = bmv080_hal_get_regs(handle, ch, 0x122, &reg, 1);
    if (status != E_BMV080_OK) return status;
    reg.value = set_field(reg.value, 8, 5, pff_min_noise_exp);
    status = bmv080_hal_set_regs(handle, ch, 0x122, &reg, 1);
    if (status != E_BMV080_OK) return status;

    /* LGCNT_INIT (0x126) and INCOMPLETE_DET_EN */
    status = bmv080_hal_get_regs(handle, ch, 0x126, &reg, 1);
    if (status != E_BMV080_OK) return status;
    reg.value = set_field(reg.value, 0, 6, pff_lagcnt_init);
    reg.value = set_field(reg.value, 8, 1, 1);
    status = bmv080_hal_set_regs(handle, ch, 0x126, &reg, 1);
    if (status != E_BMV080_OK) return status;
  }

  return bmv080_hal_set_mode(handle, E_CHANNEL_ALL, E_BMV080_STANDBY_MODE);
}

/**
 * @brief Configures the sensor for the PFF measurement, as in the Raspberry Pi example
 */
static bool configure_pff_measurement(bmv080_handle_t handle)
{
  if (!pff_step_ok("Setting sensor to standby mode", bmv080_hal_set_mode(handle, E_CHANNEL_ALL, E_BMV080_STANDBY_MODE))) return false;

  /* instead of the PFF exporter (not available in the bare build) the frames are passed to bmv080_pff_ready() */
  if (!pff_step_ok("Registering PFF callback", bmv080_hal_set_pff_callback(handle, bmv080_pff_ready, NULL))) return false;

  if (!pff_step_ok("Setting SNR threshold", bmv080_set_parameter(handle, "snr_threshold", &pff_snr_threshold))) return false;
  if (!pff_step_ok("Setting eye safety check", bmv080_hal_set_eye_safety_check(handle, true))) return false;
  if (!pff_step_ok("Setting bulk fetching", bmv080_hal_set_bulk_fetching(handle, 255, true))) return false;

  post_processor_parameter_t pp = {0};
  pp.volumetric_mass_density  = 1.6f;
  pp.integration_time         = 10;
  pp.upper_velocity_limit     = 1.5f;
  pp.do_obstruction_detection = 1;
  pp.distribution_id          = 1;
  pp.pm25_scaling_factor      = 1;
  /* the export path is not used in the bare build */
  if (!pff_step_ok("Setting configuration", bmv080_hal_set_configuration_detailed(handle, "1.0.16", "3.0.0", pp, ""))) return false;

  for (bmv080_channel_id_t ch = E_CHANNEL_ID_1; ch <= E_CHANNEL_ID_3; ch++)
  {
    if (!pff_step_ok("Setting active mesa (laser diode B)", bmv080_hal_set_active_mesa(handle, ch, E_BMV080_LASER_DIODE_B))) return false;
  }

  return true;
}

/**
 * @brief Runs a continuous measurement
 * @param duration_seconds measurement duration, <= 0 runs forever
 */
static bmv080_status_code_t run_measurement(bmv080_handle_t handle, int duration_seconds)
{
  bmv080_status_code_t status = bmv080_start_continuous_measurement(handle);
  if (status != E_BMV080_OK)
  {
    diag_log("Starting BMV080 failed with status %d", (int)status);
    return status;
  }

  const uint32_t start_ms    = get_tick_ms();
  const uint32_t duration_ms = (duration_seconds > 0) ? (uint32_t)duration_seconds * 1000u : 0u;

  bmv080_status_code_t reported_status = E_BMV080_OK;

  for(;;)
  {
    status = bmv080_serve_interrupt(handle, bmv080_data_ready, NULL);
    /* only report a change, otherwise the log would be flooded every 10 ms */
    if ((status != E_BMV080_OK) && (status != reported_status))
    {
      diag_log("Reading BMV080 failed with status %d", (int)status);
    }
    reported_status = status;

    if ((duration_ms != 0u) && ((get_tick_ms() - start_ms) >= duration_ms))
    {
      break;
    }

    bmv080_delay(PFF_POLL_DELAY_MS);
  }

  return bmv080_stop_measurement(handle);
}

/**
 * @brief Main task function for BMV080 sensor management
 * @param pvParameter FreeRTOS task parameters (unused)
 * @details Implements the complete sensor initialization and measurement sequence:
 * 1. SPI interface initialization
 * 2. Sensor version validation
 * 3. Sensor configuration (reset, duty cycling parameters)
 * 4. Continuous measurement loop with error handling
 * 5. Temperature sensor integration
 * @see bmv080_open()
 * @see bmv080_start_duty_cycling_measurement()
 */
void bmv080_task(void *pvParameter)
{

#if BMV080_USE_I2C
  /* levels first, while the pins are still plain inputs */
  i2c_check_lines();

  esp_err_t comm_status = i2c_init(&hi2c);
  if(comm_status != ESP_OK)
  {
    diag_log("Initializing the I2C communication interface failed with status %d", (int)comm_status);
    vTaskDelete(NULL);
  }
  diag_log("BMV080 on I2C port %d, SDA %d, SCL %d, address 0x%02X, internal pull ups %s",
           BMV080_I2C_PORT, BMV080_I2C_SDA_PIN, BMV080_I2C_SCL_PIN, BMV080_I2C_ADDRESS,
           BMV080_I2C_INTERNAL_PULLUP ? "on" : "off");
#else
  esp_err_t comm_status = spi_init(&hspi);
  if(comm_status != ESP_OK)
  {
    diag_log("Initializing the SPI communication interface failed with status %d", (int)comm_status);
    vTaskDelete(NULL);
  }
#endif

	uint16_t major = 0;
	uint16_t minor = 0;
	uint16_t patch = 0;
	char  	 git_hash[12] = {0};
	int32_t commits_ahead = 0;

  /* the sensor needs a moment after power up before it answers */
  bmv080_delay(5000);
  diag_log("BMV080 sensor starting on %s", shortId);

#if BMV080_USE_I2C
  i2c_scan(hi2c);
#endif

  bmv080_status_code_t bmv080_current_status = E_BMV080_OK;
  bmv080_current_status = bmv080_get_driver_version(&major, &minor, &patch, git_hash, &commits_ahead);
  if (bmv080_current_status != E_BMV080_OK)
  {
    diag_log("Getting BMV080 sensor driver version failed with BMV080 status %d", bmv080_current_status);
    gpio_set_level(R_LED_PIN, 1);
    gpio_hold_en(R_LED_PIN);
    vTaskDelete(NULL);
  }
  diag_log("BMV080 sensor driver version: %d.%d.%d.%s.%ld", major, minor, patch, git_hash, commits_ahead);
  gpio_set_level(G_LED_PIN, 1);
  gpio_hold_en(G_LED_PIN);
  bmv080_delay(1);
  gpio_set_level(G_LED_PIN, 0);
  gpio_hold_en(G_LED_PIN);

  bmv080_handle_t handle = {0};

#if BMV080_USE_I2C
  bmv080_current_status = bmv080_open(&handle, (bmv080_sercom_handle_t)&hi2c,
                                      bmv080_i2c_read_16bit, bmv080_i2c_write_16bit, bmv080_delay);
#else
  bmv080_current_status = bmv080_open(&handle, (bmv080_sercom_handle_t)hspi,
                                      bmv080_spi_read_16bit, bmv080_spi_write_16bit, bmv080_delay);
#endif
  if(bmv080_current_status != E_BMV080_OK)
  {
    diag_log("Initializing BMV080 failed with status %d", (int)bmv080_current_status);
    gpio_set_level(R_LED_PIN, 1);
    gpio_hold_en(R_LED_PIN);
    vTaskDelete(NULL);
  }
  gpio_set_level(G_LED_PIN, 1);
  gpio_hold_en(G_LED_PIN);
  bmv080_delay(1);
  gpio_set_level(G_LED_PIN, 0);
  gpio_hold_en(G_LED_PIN);

  bmv080_current_status = bmv080_reset(handle);
  if (bmv080_current_status != E_BMV080_OK)
  {
    diag_log("Resetting BMV080 sensor unit failed with BMV080 status %d", (int)bmv080_current_status);
    gpio_set_level(R_LED_PIN, 1);
    gpio_hold_en(R_LED_PIN);
    vTaskDelete(NULL);
  }
  gpio_set_level(G_LED_PIN, 1);
  gpio_hold_en(G_LED_PIN);
  bmv080_delay(1);
  gpio_set_level(G_LED_PIN, 0);
  gpio_hold_en(G_LED_PIN);

 temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
 temperature_sensor_install(&temp_sensor_config, &temp_sensor);
 temperature_sensor_enable(temp_sensor);

   /*********************************************************************************************************************
    * PFF measurement as in the Raspberry Pi example
    *********************************************************************************************************************/
  if (!configure_pff_measurement(handle))
  {
    vTaskDelete(NULL);
  }

  /* initial phase: noise floor settling */
  if (!pff_step_ok("Unfreezing noise floor", set_na_update_stopped(handle, false)))
  {
    vTaskDelete(NULL);
  }
  diag_log("Pre-measurement (noise floor settling) for %d s ...", pff_initial_time_seconds);
  if (!pff_step_ok("Pre-measurement", run_measurement(handle, pff_initial_time_seconds)))
  {
    vTaskDelete(NULL);
  }

  /* freeze noise floor */
  if (!pff_step_ok("Freezing noise floor", set_na_update_stopped(handle, true)))
  {
    vTaskDelete(NULL);
  }

  /* optional high rate configuration */
  if (pff_enable_high_rate_config)
  {
    if (!pff_step_ok("Applying high rate configuration", apply_high_rate_config(handle)))
    {
      vTaskDelete(NULL);
    }
  }

  /* main measurement, frames are streamed over Wi-Fi */
  diag_log("Main measurement, streaming PFF frames over Wi-Fi ...");
  gpio_set_level(G_LED_PIN, 1);
  gpio_hold_en(G_LED_PIN);
  bmv080_delay(1);
  gpio_set_level(G_LED_PIN, 0);
  gpio_hold_en(G_LED_PIN);

  (void)pff_step_ok("Main measurement", run_measurement(handle, -1));
  vTaskDelete(NULL);
}

/**
 * @brief Starts BMV080 monitoring task
 * @details Creates FreeRTOS task with dedicated stack space for sensor operations
 * @note Task priority set to configMAX_PRIORITIES - 1 for high priority handling
 */
void bmv080_app_start()
{
  pff_queue = xQueueCreate(PFF_QUEUE_LENGTH, sizeof(bmv080_pff_data_t));
  xTaskCreate(&pff_net_task, "pff_net_task", 6 * 1024, NULL, 5, NULL);
  xTaskCreate(&bmv080_task, "bmv080_task", 60 * 1024, NULL, configMAX_PRIORITIES - 1, NULL);
}
