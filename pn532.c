// PN532 functions
static const char TAG[] = "PN532";

#include "pn532.h"
#include "esp_log.h"
#include <driver/uart.h>

#define	HEXLOG ESP_LOG_INFO

struct pn532_s
{
   uint8_t uart;                // Which UART
   uint8_t tx;                  // Tx GPIO
   uint8_t rx;                  // Rx GPIO
};

static int
uart_rx (pn532_t * p, uint8_t * buf, uint32_t length, int ms)
{ // Low level UART rx with optional logging
   ms /= portTICK_PERIOD_MS;
   if (!ms)
      ms = 1;
   int l = uart_read_bytes (p->uart, buf, length, ms);
#ifdef HEXLOG
   if (l > 0)
      ESP_LOG_BUFFER_HEX_LEVEL ("NFCRx", buf, l, HEXLOG);
#endif
   return l;
}

static int
uart_tx (pn532_t * p, const uint8_t * src, size_t size)
{ // Low level UART tx with optional logging
   int l = uart_write_bytes (p->uart, (char *) src, size);
#ifdef HEXLOG
   if (l > 0)
      ESP_LOG_BUFFER_HEX_LEVEL ("NFCTx", src, l, HEXLOG);
#endif
   return l;
}

void *
pn532_end (pn532_t * p)
{                               // Close
   if (p)
   {
      uart_driver_delete (p->uart);
      free (p);
   }
   return NULL;
}

pn532_t *
pn532_init (int uart, int tx, int rx, uint8_t p3)
{                               // Init PN532
   pn532_t *p = malloc (sizeof (*p));
   if (!p)
      return p;
   p->uart = uart;
   p->tx = tx;
   p->rx = rx;
   // Init UART
   uart_config_t uart_config = {
      .baud_rate = 115200,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
   };
   esp_err_t err;
   if ((err = uart_param_config (uart, &uart_config)) || (err = uart_set_pin (uart, tx, rx, -1, -1))
       || (err = uart_driver_install (uart, 1024, 1024, 0, NULL, 0)))
   {
      ESP_LOGE (TAG, "UART fail %s", esp_err_to_name (err));
      free (p);
      return NULL;
   }
   ESP_LOGD(TAG,"PN532 UART %d Tx %d Rx %d",uart,tx,rx);
   uint8_t buf[8];
   // Poke serial
   buf[0] = 0x55;
   buf[1] = 0x55;
   buf[2] = 0x00;
   buf[3] = 0x00;
   buf[4] = 0x00;
   uart_tx (p, buf, 5);
   // TODO define what is returned from these functions
   // Set up PN532
   buf[0] = 0x02;               // GetFirmwareVersion
   if (pn532_tx (p, 0, NULL, 1, buf) < 0 || pn532_rx (p, 0, NULL, sizeof (buf), buf) < 0)
      return pn532_end (p);
   //uint32_t ver = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
   buf[0] = 0x32;               // RFConfiguration
   buf[1] = 5;                  // Config item 5 (MaxRetries)
   buf[2] = 0xFF;               // MxRtyATR (default = 0xFF)
   buf[3] = 0x01;               // MxRtyPSL (default = 0x01)
   buf[4] = 0x01;               // MxRtyPassiveActivation
   if (pn532_tx (p, 0, NULL, 5, buf) < 0 || pn532_rx (p, 0, NULL, sizeof (buf), buf) < 0)
      return pn532_end (p);
   buf[0] = 0x14;               // SAMConfiguration
   buf[1] = 0x01;               // Normal
   buf[2] = 20;                 // *50ms timeout
   buf[3] = 0x01;               // Use IRQ
   if (pn532_tx (p, 0, NULL, 4, buf) < 0 || pn532_rx (p, 0, NULL, sizeof (buf), buf) < 0)
      return pn532_end (p);
   buf[0] = 0x08;               // WriteRegister
   buf[1] = 0xFF;               // P3CFGB
   buf[2] = 0xFD;               // P3CFGB
   buf[3] = p3;                 // Define output bits
   buf[4] = 0xFF;               // P3
   buf[5] = 0xB0;               // P3
   buf[6] = 0xFF;               // All high
   if (pn532_tx (p, 0, NULL, 7, buf) < 0 || pn532_rx (p, 0, NULL, sizeof (buf), buf) < 0)
      return pn532_end (p);
   buf[0] = 0x32;               // RFConfiguration
   buf[1] = 0x04;               // MaxRtyCOM
   buf[2] = 1;                  // Retries (default 0)
   if (pn532_tx (p, 0, NULL, 3, buf) < 0 || pn532_rx (p, 0, NULL, sizeof (buf), buf) < 0)
      return pn532_end (p);
   buf[0] = 0x32;               // RFConfiguration
   buf[1] = 0x02;               // Various timings (100*2^(n-1))us
   buf[2] = 0x00;               // RFU
   buf[3] = 0x0B;               // Default (102.4 ms)
   buf[4] = 0x0A;               // Default is 0x0A (51.2 ms)
   if (pn532_tx (p, 0, NULL, 5, buf) || pn532_rx (p, 0, NULL, sizeof (buf), buf) < 0)
      return pn532_end (p);
   return p;
}

// Low level access functions
int
pn532_tx (pn532_t * p, int len1, uint8_t * data1, int len2, uint8_t * data2)
{                               // Send data to PN532
   uint8_t buf[20],
    *b = buf;
   *b++ = 0x00;                 // Preamble
   *b++ = 0x00;                 // Start 1
   *b++ = 0xFF;                 // Start 2
   int l = len1 + len2 + 1;
   if (l >= 0x100)
   {
      *b++ = 0xFF;              // Extended len
      *b++ = 0xFF;
      *b++ = (l >> 8);          // len
      *b++ = (l & 0xFF);
      *b++ = -(l >> 8) - (l & 0xFF);    // Checksum
   } else
   {
      *b++ = l;                 // Len
      *b++ = -l;                // Checksum
   }
   *b++ = 0xD4;                 // Direction (host to PN532)
   uint8_t sum = 0;
   int i;
   for (i = 0; i < len1; i++)
      sum += data1[i];
   for (i = 0; i < len2; i++)
      sum += data2[i];
   // Send data
   uart_flush (p->uart);
   uart_tx (p, buf, b - buf);
   if (len1)
      uart_tx (p, data1, len1);
   if (len2)
      uart_tx (p, data2, len2);
   buf[0] = -sum;               // Checksum
   buf[1] = 0x00;               // Postamble
   uart_tx (p, buf, 2);
   i = uart_rx (p, buf, 6, 20);
   if (i < 6 || buf[0] || buf[1] || buf[2] != 0xFF || buf[3] || buf[4] != 0xFF || buf[5])
      return -1;                // Bad
   return 0;                    // OK
}

int
pn532_rx (pn532_t * p, int max1, uint8_t * data1, int max2, uint8_t * data2)
{                               // Recv data from PN532
   uint8_t buf[9];
   int l = uart_rx (p, buf, 6, 100);
   if (l < 6 || buf[0] || buf[1] || buf[2] != 0xFF)
      return -1;
   int len = buf[3];
   if (buf[3] == 0xFF && buf[4] == 0xFF)
   {                            // Extended
      l = uart_rx (p, buf + 6, 3, 20);
      if (l != 6)
         return -1;
      if ((uint8_t) (buf[5] + buf[6] + buf[7]))
         return -2;             // Bad checksum
      len = (buf[5] << 8) + buf[6];
      if (buf[8] != 0xD5)
         return -3;             // Not reply
   } else
   {                            // Normal
      if ((uint8_t) (buf[3] + buf[4]))
         return -4;             // Bad checksum
      len = buf[3];
      if (buf[5] != 0xD5)
         return -5;             // Not reply
   }
   if (!len)
      return -6;                // Invalue
   len--;
   uint8_t sum = 0;
   if (len > max1 + max2)
      return -7;                // Too big
   l = max1;
   if (l > len)
      l = len;
   if (l)
   {
      if (uart_read_bytes (p->uart, data1, l, 20) != l)
         return -8;             // Bad read
      ESP_LOG_BUFFER_HEX_LEVEL (TAG, data1, l, ESP_LOG_INFO);
      len -= l;
      while (l)
         sum += data1[--l];
   }
   l = max2;
   if (l > len)
      l = len;
   if (l)
   {
      if (uart_read_bytes (p->uart, data2, l, 20) != l)
         return -9;             // Bad read
      ESP_LOG_BUFFER_HEX_LEVEL (TAG, data2, l, ESP_LOG_INFO);
      len -= l;
      while (l)
         sum += data1[--l];
   }
   l = uart_rx (p, buf, 2, 20);
   ESP_LOG_BUFFER_HEX_LEVEL (TAG, buf, l, ESP_LOG_INFO);
   if (l != 2)
      return -10;               // Postable
   if ((uint8_t) (buf[0] + sum))
      return -11;               // checksum
   if (buf[1])
      return -12;               // postable
   return len;
}

// Data exchange (for DESFire use)
int
pn532_dx (pn532_t * p, unsigned int len, uint8_t * data, unsigned int max)
{                               // Card access function - sends to card starting CMD byte, and receives reply in to same buffer, starting status byte, returns len
   // TODO
   return -1;
}

// Other higher level functions
