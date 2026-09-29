/**
 * @file diag.c
 * @brief Log buffer for the status page
 *
 * The board normally runs without a USB cable, so the serial monitor is not
 * available. Every message that used to go to the console is therefore also kept in memory
 * and can be read over Wi-Fi at http://192.168.4.1/status
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "diag.h"

/** @brief Kept log lines, the line with number n is stored at index n % DIAG_LINES */
static char lines[DIAG_LINES][DIAG_LINE_LENGTH];
/** @brief Number the next line gets */
static uint32_t next_line = 0;
/** @brief Protects the buffer between the sensor task and the web server task */
static SemaphoreHandle_t mutex = NULL;

void diag_init(void)
{
  mutex = xSemaphoreCreateMutex();
}

void diag_log(const char *format, ...)
{
  char text[DIAG_LINE_LENGTH];

  va_list arguments;
  va_start(arguments, format);
  int length = vsnprintf(text, sizeof(text), format, arguments);
  va_end(arguments);

  if (length < 0)
  {
    return;
  }

  /* the console is only connected while the board hangs on the USB cable */
  printf("%s\r\n", text);

  if (mutex == NULL)
  {
    return;
  }

  xSemaphoreTake(mutex, portMAX_DELAY);
  strncpy(lines[next_line % DIAG_LINES], text, DIAG_LINE_LENGTH - 1);
  lines[next_line % DIAG_LINES][DIAG_LINE_LENGTH - 1] = '\0';
  next_line++;
  xSemaphoreGive(mutex);
}

size_t diag_dump(char *buffer, size_t size)
{
  if ((buffer == NULL) || (size == 0) || (mutex == NULL))
  {
    return 0;
  }

  size_t used = 0;
  uint32_t oldest = (next_line > DIAG_LINES) ? (next_line - DIAG_LINES) : 0;

  xSemaphoreTake(mutex, portMAX_DELAY);
  for (uint32_t number = oldest; number < next_line; number++)
  {
    int written = snprintf(&buffer[used], size - used, "%s\n", lines[number % DIAG_LINES]);
    if ((written <= 0) || ((used + (size_t)written) >= size))
    {
      break;
    }
    used += (size_t)written;
  }
  xSemaphoreGive(mutex);

  return used;
}
