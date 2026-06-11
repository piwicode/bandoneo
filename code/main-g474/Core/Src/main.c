/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "console.h"
#include "usb_app.h"
#include "spi_link.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
ADC_HandleTypeDef hadc3;
ADC_HandleTypeDef hadc4;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef hdma_spi1_rx;
DMA_HandleTypeDef hdma_spi2_rx;

UART_HandleTypeDef huart1;

PCD_HandleTypeDef hpcd_USB_FS;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
static void MX_ADC3_Init(void);
static void MX_ADC4_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USB_PCD_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void delay_us(uint32_t us)
{
  uint32_t cycles = us * (SystemCoreClock / 1000000U);
  uint32_t start = DWT->CYCCNT;
  while ((DWT->CYCCNT - start) < cycles);
}

/* Sends a string over SWO ITM port 0; no-op if no debugger has enabled tracing */
static void swo_print(const char *s)
{
  while (*s)
  {
    if ((ITM->TCR & ITM_TCR_ITMENA_Msk) && (ITM->TER & (1UL << 0)))
    {
      while (ITM->PORT[0].u32 == 0);
      ITM->PORT[0].u8 = (uint8_t)*s;
    }
    s++;
  }
}

/* ===== Wing keyboard link (SPI slave) ===================================== *
 * Each wing streams a fixed SPI_LINK_FRAME_WORDS frame (see spi_link.h): word
 * 0 is the wing id, words 1.. are the raw hall measurement per key. SPI1 is
 * wired to the left wing, SPI2 to the right (L_SPI_ / R_SPI_ pins), but the
 * wing id in the frame is authoritative for note mapping. Reception is
 * DMA-driven (interrupt fallback), re-armed from the main loop on the NSS idle
 * gap (the only point that word-aligns); bad (CRC/overrun/misaligned) frames
 * are dropped and counted. */

/* Known wing ids, both 142-key "Rheinische Lage". */
#define WING_ID_RIGHT 1
#define WING_ID_LEFT  2

/* Press/release hysteresis: a hall reading falls as a key is pressed and rises
 * back as it is released. release_threshold > press_threshold gives a dead band
 * so a key resting near the trip point doesn't chatter. */
#define KEYBOARD_PRESS_THRESHOLD   1900
#define KEYBOARD_RELEASE_THRESHOLD 2100

typedef enum { BELLOWS_PULL = 0, BELLOWS_PUSH = 1 } bellows_t;

/* The bandoneon is bisonoric: each key sounds a different note on push vs pull.
 * Until push/pull sensing is wired from the motherboard's two hall sensors
 * (hall0/hall1 in the main loop), treat every note as pull. Follow-up: derive
 * this from those readings and update g_bellows. */
static bellows_t g_bellows = BELLOWS_PULL;

/* Maps (wing, key, bellows) to a MIDI note number, 0 = no note.
 *
 * Placeholder: the real "Rheinische Lage" layout assigns a specific note to
 * each combination; until that table is digitized we use simple per-wing/
 * bellows offsets so the link, hysteresis and push/pull plumbing can be
 * exercised. Replace the body with per-wing [bellows][key] lookup tables. */
static uint8_t map_note(uint8_t wing_id, int key, bellows_t bellows)
{
  int base;
  switch (wing_id)
  {
    case WING_ID_RIGHT: base = 60; break;  /* right hand: melody side, ~C4 */
    case WING_ID_LEFT:  base = 36; break;  /* left hand: bass side, ~C2 */
    default: return 0;                     /* unknown wing -> no note */
  }
  if (bellows == BELLOWS_PUSH) base += 1; /* placeholder push/pull split */
  int note = base + key;
  return (note > 0 && note < 128) ? (uint8_t)note : 0;
}

typedef struct
{
  SPI_HandleTypeDef *hspi;
  const char *name;
  GPIO_TypeDef *nss_port;   /* NSS line, read to find the inter-frame gap */
  uint16_t nss_pin;

  /* Double buffer: the ISR fills rx[rx_active]; on completion it flips and
   * hands the finished buffer to the main loop via ready/ready_buf. */
  uint16_t rx[2][SPI_LINK_FRAME_WORDS];
  volatile uint8_t rx_active;
  volatile uint8_t ready;
  volatile uint8_t ready_buf;
  volatile uint8_t needs_resync;

  /* Per-reception outcome counters (updated in interrupt context). Every
   * reception is classified as exactly one of these, so
   * good + misaligned + crc_err + bus_err = total receptions:
   *   good       - completed and word 0 is a known wing id -> decoded
   *   misaligned - completed but word 0 is NOT a wing id (the 16-bit word
   *                boundaries don't line up with the wing's frame; the
   *                hardware CRC did not reject it) -> dropped
   *   crc_err    - HAL reported a CRC mismatch
   *   bus_err    - HAL reported overrun / mode / frame error */
  volatile uint32_t rx_good;
  volatile uint32_t rx_misaligned;
  volatile uint32_t crc_err;
  volatile uint32_t bus_err;
  volatile uint8_t  last_good_wing;  /* word 0 of the last good reception */
  volatile uint16_t last_bad_word0;  /* word 0 of the last misaligned reception */

  /* Per-key decode state (main-loop only). */
  uint16_t key_min[SPI_LINK_NUM_KEYS];
  uint8_t  key_min_init;
  uint8_t  key_pressed[SPI_LINK_NUM_KEYS];

  /* Snapshots for the 1 Hz rate report and the throttled error log. */
  uint32_t last_good, last_misaligned, last_crc, last_bus;
  uint32_t logged_crc, logged_bus, logged_misaligned, last_err_log_tick;
} spi_bus_t;

static spi_bus_t g_bus[2];

static spi_bus_t *bus_from_hspi(SPI_HandleTypeDef *hspi)
{
  if (hspi == &hspi1) return &g_bus[0];
  if (hspi == &hspi2) return &g_bus[1];
  return NULL;
}

/* Starts one frame reception. Uses RX DMA when the SPI has a linked DMA channel
 * (hdmarx), so the frame is stored without per-word CPU work and the link keeps
 * up at high SCK; falls back to interrupt mode if DMA isn't wired yet (e.g.
 * before the .ioc SPIx_RX DMA request is regenerated). The control flow is
 * identical either way: completion/CRC/overrun land in the same HAL callbacks. */
static HAL_StatusTypeDef bus_rearm(spi_bus_t *b)
{
  if (b->hspi->hdmarx != NULL)
    return HAL_SPI_Receive_DMA(b->hspi, (uint8_t *)b->rx[b->rx_active], SPI_LINK_FRAME_WORDS);
  return HAL_SPI_Receive_IT(b->hspi, (uint8_t *)b->rx[b->rx_active], SPI_LINK_FRAME_WORDS);
}

static int wing_id_known(uint8_t id)
{
  return id == WING_ID_RIGHT || id == WING_ID_LEFT;
}

/* Completion and error callbacks never re-arm directly: re-arming the instant a
 * frame ends lands the next receive mid-stream (the wing is still finishing /
 * NSS hasn't returned high), so it comes back word-misaligned. Instead they
 * classify/count the reception and flag needs_resync; the main loop re-arms on
 * the next NSS idle gap, which is the only point that reliably word-aligns.
 *
 * RxCplt classifies here (not in the main loop) so every reception is counted
 * even when the loop is too slow to drain each one: a frame is "good" only if
 * word 0 is a known wing id, otherwise "misaligned" (dropped). Only good frames
 * are handed to the loop for decoding. */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
  spi_bus_t *b = bus_from_hspi(hspi);
  if (!b) return;
  uint16_t w0 = b->rx[b->rx_active][0];
  if (wing_id_known((uint8_t)w0))
  {
    b->rx_good++;
    b->last_good_wing = (uint8_t)w0;
    b->ready_buf = b->rx_active;
    b->ready = 1;
  }
  else
  {
    b->rx_misaligned++;
    b->last_bad_word0 = w0;
  }
  b->needs_resync = 1;
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  spi_bus_t *b = bus_from_hspi(hspi);
  if (!b) return;
  uint32_t ec = hspi->ErrorCode;
  hspi->ErrorCode = HAL_SPI_ERROR_NONE;
  if (ec == HAL_SPI_ERROR_CRC) b->crc_err++;
  else                         b->bus_err++;
  b->needs_resync = 1;
}

/* Decodes one received frame into note on/off events (printed and sent over
 * USB MIDI). Channels whose since-boot minimum is still 0 are treated as
 * unpopulated and skipped, matching the wing's present-key detection. */
static void bus_process_frame(spi_bus_t *b, const uint16_t *frame)
{
  uint8_t wing_id = (uint8_t)frame[0];
  const uint16_t *meas = &frame[1];

  for (int k = 0; k < SPI_LINK_NUM_KEYS; k++)
  {
    uint16_t v = meas[k];
    if (!b->key_min_init || v < b->key_min[k]) b->key_min[k] = v;
    if (b->key_min[k] == 0) continue;          /* unpopulated channel */

    uint8_t note = map_note(wing_id, k, g_bellows);
    if (note == 0) continue;

    if (!b->key_pressed[k] && v <= KEYBOARD_PRESS_THRESHOLD)
    {
      b->key_pressed[k] = 1;
      printf("NOTE ON  %s wing=%u key=%2d note=%3u val=%4u\r\n",
             b->name, wing_id, k, note, v);
      usb_app_midi_note_on(note, 100);
    }
    else if (b->key_pressed[k] && v >= KEYBOARD_RELEASE_THRESHOLD)
    {
      b->key_pressed[k] = 0;
      printf("NOTE OFF %s wing=%u key=%2d note=%3u val=%4u\r\n",
             b->name, wing_id, k, note, v);
      usb_app_midi_note_off(note);
    }
  }
  b->key_min_init = 1;
}

/* Decodes the latest good frame the ISR handed over (RxCplt only flags ready
 * for word-aligned frames with a valid wing id). */
static void bus_poll(spi_bus_t *b)
{
  if (!b->ready) return;
  uint8_t idx = b->ready_buf;
  b->ready = 0;
  uint16_t frame[SPI_LINK_FRAME_WORDS];
  memcpy(frame, b->rx[idx], sizeof(frame));
  bus_process_frame(b, frame);
}

/* Arms the next reception, aligned to a frame boundary. This is the only place
 * reception is (re)armed: after every completion/error the callbacks set
 * needs_resync and this runs from the main loop, arming only while NSS is idle
 * high (the inter-frame gap) so the transfer starts at word 0 of the next
 * frame. Throughput is therefore bounded by the loop rate, but every captured
 * frame is word-aligned. */
static void bus_service_resync(spi_bus_t *b)
{
  if (!b->needs_resync) return;
  if (HAL_GPIO_ReadPin(b->nss_port, b->nss_pin) == GPIO_PIN_RESET) return;

  /* Reset the slave's bit framing and CRC for the next frame. A fresh 16-bit
   * word count and SPI_RESET_CRC (which toggles CRCEN) only take effect across
   * an SPE off->on transition (RM0440). If SPE just stays on between frames the
   * slave free-runs its bit counter (frames come back bit-misaligned) and the
   * CRC never resets (so it can't reject them). Drop SPE and flush any stale RX
   * here; HAL_SPI_Receive_DMA turns SPE back on and resets the CRC. Safe because
   * needs_resync is only set once a transfer has ended (state READY, DMA idle). */
  __HAL_SPI_DISABLE(b->hspi);
  while (__HAL_SPI_GET_FLAG(b->hspi, SPI_FLAG_RXNE))
    (void)b->hspi->Instance->DR;
  __HAL_SPI_CLEAR_OVRFLAG(b->hspi);

  if (HAL_GPIO_ReadPin(b->nss_port, b->nss_pin) == GPIO_PIN_RESET) return;
  if (bus_rearm(b) == HAL_OK)
    b->needs_resync = 0;
}

/* Logs newly accumulated faults at most once per window, so a storm on a
 * misaligned/overrunning bus can't flood the console. "misaligned" means a
 * frame arrived whose word 0 wasn't a known wing id (1/2). */
#define SPI_ERR_LOG_THROTTLE_MS 500
static void bus_log_errors(spi_bus_t *b)
{
  uint32_t mis = b->rx_misaligned, crc = b->crc_err, bus = b->bus_err;
  uint32_t new_mis = mis - b->logged_misaligned;
  uint32_t new_crc = crc - b->logged_crc;
  uint32_t new_bus = bus - b->logged_bus;
  if (new_mis == 0 && new_crc == 0 && new_bus == 0) return;
  uint32_t now = HAL_GetTick();
  if (now - b->last_err_log_tick < SPI_ERR_LOG_THROTTLE_MS) return;
  b->last_err_log_tick = now;
  b->logged_misaligned = mis;
  b->logged_crc = crc;
  b->logged_bus = bus;
  printf("SPI %s: +%lu misaligned (last word0=%u, want wing id 1/2), +%lu crc-err, +%lu bus-err\r\n",
         b->name, (unsigned long)new_mis, (unsigned)b->last_bad_word0,
         (unsigned long)new_crc, (unsigned long)new_bus);
}

/* 1 Hz per-bus reception rates. good + misaligned + crc-err + bus-err = total
 * receptions; "good" are the only ones decoded into notes. */
static void bus_print_rates(spi_bus_t *b, uint32_t dt_ms)
{
  uint32_t good = b->rx_good, mis = b->rx_misaligned, crc = b->crc_err, bus = b->bus_err;
  uint32_t d_good = good - b->last_good;
  uint32_t d_mis  = mis  - b->last_misaligned;
  uint32_t d_crc  = crc  - b->last_crc;
  uint32_t d_bus  = bus  - b->last_bus;
  b->last_good = good;
  b->last_misaligned = mis;
  b->last_crc = crc;
  b->last_bus = bus;
  printf("%s: good: %lu/s, misaligned: %lu/s, crc-err: %lu/s, bus-err: %lu/s "
         "(last good wing=%u, last bad word0=%u)\r\n",
         b->name,
         (unsigned long)(d_good * 1000u / dt_ms),
         (unsigned long)(d_mis  * 1000u / dt_ms),
         (unsigned long)(d_crc  * 1000u / dt_ms),
         (unsigned long)(d_bus  * 1000u / dt_ms),
         b->last_good_wing, (unsigned)b->last_bad_word0);
}

static void spi_link_init(void)
{
  g_bus[0].hspi = &hspi1; g_bus[0].name = "SPI1/L";
  g_bus[0].nss_port = L_SPI_NSS_GPIO_Port; g_bus[0].nss_pin = L_SPI_NSS_Pin;
  g_bus[1].hspi = &hspi2; g_bus[1].name = "SPI2/R";
  g_bus[1].nss_port = R_SPI_NSS_GPIO_Port; g_bus[1].nss_pin = R_SPI_NSS_Pin;
  for (int i = 0; i < 2; i++)
  {
    g_bus[i].rx_active = 0;
    g_bus[i].needs_resync = 1;   /* armed by bus_service_resync() on the idle gap */
  }
}

int console_execute(int argc, const char * const *argv)
{
  if (argc == 0)
    return 0;
  if (strcmp(argv[0], "hello") == 0)
  {
    printf("Hello, Bandoneo!\r\n");
  }
  else if (strcmp(argv[0], "midi") == 0)
  {
    /* midi [note]: send a test note on/off pair over USB MIDI (default A4) */
    uint8_t note = (argc > 1) ? (uint8_t)atoi(argv[1]) : 69;
    usb_app_midi_test_note(note);
    printf("Sent MIDI note %u on/off\r\n", note);
  }
  else
    printf("Unknown command: %s\r\n", argv[0]);
  return 0;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_ADC4_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_USART1_UART_Init();
  MX_USB_PCD_Init();
  /* USER CODE BEGIN 2 */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
  console_init(&huart1, USART1_IRQn);
  usb_app_init();
  spi_link_init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint8_t fn_prev = 0xFF;
  uint8_t exp_present_prev = 0xFF, sus_present_prev = 0xFF;
  uint32_t exp_val_prev = 0xFFFF, sus_val_prev = 0xFFFF;
  uint32_t hall0_prev = 0xFFFF, hall1_prev = 0xFFFF;
  uint32_t last_rate_tick = HAL_GetTick();
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    usb_app_task();

    /* Decode any wing frames received since the last iteration, recover any bus
     * that lost alignment, then surface link errors (throttled) and a 1 Hz
     * frame/error rate per bus. */
    for (int i = 0; i < 2; i++)
    {
      bus_poll(&g_bus[i]);
      bus_service_resync(&g_bus[i]);
      bus_log_errors(&g_bus[i]);
    }
    uint32_t rate_now = HAL_GetTick();
    uint32_t rate_dt = rate_now - last_rate_tick;
    if (rate_dt >= 1000)
    {
      last_rate_tick = rate_now;
      for (int i = 0; i < 2; i++)
        bus_print_rates(&g_bus[i], rate_dt);
    }

    uint8_t fn = 0;
    if (HAL_GPIO_ReadPin(SW_FN0_BOOT0_GPIO_Port, SW_FN0_BOOT0_Pin) == GPIO_PIN_RESET) fn |= 1;
    if (HAL_GPIO_ReadPin(SW_FN1_GPIO_Port,       SW_FN1_Pin)        == GPIO_PIN_RESET) fn |= 2;
    if (HAL_GPIO_ReadPin(SW_FN2_GPIO_Port,       SW_FN2_Pin)        == GPIO_PIN_RESET) fn |= 4;
    if (fn != fn_prev)
    {
      fn_prev = fn;
      printf("FN: %c%c%c\r\n",
             (fn & 1) ? '1' : '0',
             (fn & 2) ? '1' : '0',
             (fn & 4) ? '1' : '0');
    }

    uint8_t exp_present = HAL_GPIO_ReadPin(EXP_PEDAL_INT_GPIO_Port, EXP_PEDAL_INT_Pin) == GPIO_PIN_SET;
    uint8_t sus_present = HAL_GPIO_ReadPin(SUS_PEDAL_INT_GPIO_Port, SUS_PEDAL_INT_Pin) == GPIO_PIN_SET;

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1);
    uint32_t exp_val = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Start(&hadc2);
    HAL_ADC_PollForConversion(&hadc2, 1);
    uint32_t sus_val = HAL_ADC_GetValue(&hadc2);

    uint32_t exp_delta = exp_val > exp_val_prev ? exp_val - exp_val_prev : exp_val_prev - exp_val;
    uint32_t sus_delta = sus_val > sus_val_prev ? sus_val - sus_val_prev : sus_val_prev - sus_val;

    if (exp_present != exp_present_prev || (!exp_present && exp_delta > 16))
    {
      exp_present_prev = exp_present;
      exp_val_prev = exp_val;
      printf("EXP present=%u val=%u\r\n", exp_present, (unsigned)exp_val);
    }
    if (sus_present != sus_present_prev || (!sus_present && sus_delta > 16))
    {
      sus_present_prev = sus_present;
      sus_val_prev = sus_val;
      printf("SUS: present=%u val=%u\r\n", sus_present, (unsigned)sus_val);
    }

    HAL_GPIO_WritePin(HALL_NEN_GPIO_Port, HALL_NEN_Pin, GPIO_PIN_RESET);
    /* Settling budget: gate RC (R5||R6 * Ciss_Q1 = 909R * 130pF, 5t~600ns) +
     * VDDH caps (RDS_on_Q1 * (CP1+CP2) = ~120mO * 200nF, 5t~120ns) +
     * SC4015SO power-on start <1us (datasheet) => worst case <3us; 5us = ~1.7x margin. */
    delay_us(5);

    HAL_ADC_Start(&hadc3);
    HAL_ADC_PollForConversion(&hadc3, 10);
    uint32_t hall0 = HAL_ADC_GetValue(&hadc3);

    HAL_ADC_Start(&hadc4);
    HAL_ADC_PollForConversion(&hadc4, 10);
    uint32_t hall1 = HAL_ADC_GetValue(&hadc4);

    HAL_GPIO_WritePin(HALL_NEN_GPIO_Port, HALL_NEN_Pin, GPIO_PIN_SET);

    uint32_t hall0_delta = hall0 > hall0_prev ? hall0 - hall0_prev : hall0_prev - hall0;
    uint32_t hall1_delta = hall1 > hall1_prev ? hall1 - hall1_prev : hall1_prev - hall1;
    if (hall0_delta > 16 || hall1_delta > 16)
    {
      hall0_prev = hall0;
      hall1_prev = hall1;
      printf("HALL0=%u HALL1=%u\r\n", (unsigned)hall0, (unsigned)hall1);
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_CRSInitTypeDef pInit = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 12;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV4;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the SYSCFG APB clock
  */
  __HAL_RCC_CRS_CLK_ENABLE();

  /** Configures CRS
  */
  pInit.Prescaler = RCC_CRS_SYNC_DIV1;
  pInit.Source = RCC_CRS_SYNC_SOURCE_USB;
  pInit.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
  pInit.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000,1000);
  pInit.ErrorLimitValue = 34;
  pInit.HSI48CalibrationValue = 32;

  HAL_RCCEx_CRSConfig(&pInit);
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.GainCompensation = 0;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.GainCompensation = 0;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.DMAContinuousRequests = DISABLE;
  hadc2.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc2.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief ADC3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC3_Init(void)
{

  /* USER CODE BEGIN ADC3_Init 0 */

  /* USER CODE END ADC3_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */

  /* USER CODE END ADC3_Init 1 */

  /** Common config
  */
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc3.Init.Resolution = ADC_RESOLUTION_12B;
  hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc3.Init.GainCompensation = 0;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc3.Init.LowPowerAutoWait = DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.DMAContinuousRequests = DISABLE;
  hadc3.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc3.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc3, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */

  /* USER CODE END ADC3_Init 2 */

}

/**
  * @brief ADC4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC4_Init(void)
{

  /* USER CODE BEGIN ADC4_Init 0 */

  /* USER CODE END ADC4_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC4_Init 1 */

  /* USER CODE END ADC4_Init 1 */

  /** Common config
  */
  hadc4.Instance = ADC4;
  hadc4.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc4.Init.Resolution = ADC_RESOLUTION_12B;
  hadc4.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc4.Init.GainCompensation = 0;
  hadc4.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc4.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc4.Init.LowPowerAutoWait = DISABLE;
  hadc4.Init.ContinuousConvMode = DISABLE;
  hadc4.Init.NbrOfConversion = 1;
  hadc4.Init.DiscontinuousConvMode = DISABLE;
  hadc4.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc4.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc4.Init.DMAContinuousRequests = DISABLE;
  hadc4.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc4.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC4_Init 2 */

  /* USER CODE END ADC4_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_SLAVE;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_HARD_INPUT;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_ENABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_16BIT;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_SLAVE;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_HARD_INPUT;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_ENABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_16BIT;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 921600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USB Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_PCD_Init(void)
{

  /* USER CODE BEGIN USB_Init 0 */

  /* USER CODE END USB_Init 0 */

  /* USER CODE BEGIN USB_Init 1 */

  /* USER CODE END USB_Init 1 */
  hpcd_USB_FS.Instance = USB;
  hpcd_USB_FS.Init.dev_endpoints = 8;
  hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_FS.Init.Sof_enable = DISABLE;
  hpcd_USB_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_Init 2 */

  /* USER CODE END USB_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(HALL_NEN_GPIO_Port, HALL_NEN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(L_BOOT0_GPIO_Port, L_BOOT0_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, L_NRST_Pin|R_NRST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, R_BOOT0_Pin|LED_FN2_Pin|LED_FN1_Pin|LED_FN0_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : EXP_PEDAL_INT_Pin SUS_PEDAL_INT_Pin */
  GPIO_InitStruct.Pin = EXP_PEDAL_INT_Pin|SUS_PEDAL_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : HALL_NEN_Pin */
  GPIO_InitStruct.Pin = HALL_NEN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(HALL_NEN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : L_BOOT0_Pin */
  GPIO_InitStruct.Pin = L_BOOT0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(L_BOOT0_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : L_READY_Pin */
  GPIO_InitStruct.Pin = L_READY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(L_READY_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : L_NRST_Pin R_NRST_Pin */
  GPIO_InitStruct.Pin = L_NRST_Pin|R_NRST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : R_BOOT0_Pin LED_FN2_Pin LED_FN1_Pin LED_FN0_Pin */
  GPIO_InitStruct.Pin = R_BOOT0_Pin|LED_FN2_Pin|LED_FN1_Pin|LED_FN0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : R_READY_Pin */
  GPIO_InitStruct.Pin = R_READY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(R_READY_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PGM_DETECT_Pin */
  GPIO_InitStruct.Pin = PGM_DETECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(PGM_DETECT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SW_FN2_Pin SW_FN1_Pin */
  GPIO_InitStruct.Pin = SW_FN2_Pin|SW_FN1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : SW_FN0_BOOT0_Pin */
  GPIO_InitStruct.Pin = SW_FN0_BOOT0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SW_FN0_BOOT0_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
