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
#include <string.h>
#include "console.h"
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
ADC_HandleTypeDef hadc5;
DMA_HandleTypeDef hdma_adc1;
DMA_HandleTypeDef hdma_adc2;
DMA_HandleTypeDef hdma_adc3;
DMA_HandleTypeDef hdma_adc4;
DMA_HandleTypeDef hdma_adc5;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;

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
static void MX_ADC5_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static uint16_t adc_read_channel(ADC_HandleTypeDef *hadc, uint32_t channel);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define HALL_NUM_SEL   4
#define HALL_NUM_ADC   5
#define HALL_NUM_RANK  2
// Slots per ADC across a full sweep: one (sel, rank) pair each.
#define HALL_SLOTS_PER_ADC (HALL_NUM_SEL * HALL_NUM_RANK)
#define HALL_DEAD_ZONE 64
// Settling delay after switching the SEL mux, in spin-loop iterations.
#define MUX_LATENCY_CYCLES 160

// Layout matches the per-ADC DMA buffer: [adc][sel*HALL_NUM_RANK + rank].
static uint16_t g_hall_data[HALL_NUM_ADC][HALL_SLOTS_PER_ADC];
static uint16_t g_hall_last_reported[HALL_NUM_ADC][HALL_SLOTS_PER_ADC];

// Constant across all calls; only Channel changes, so keep one instance and
// avoid re-zeroing + re-writing the fixed fields on every conversion setup.
static ADC_ChannelConfTypeDef adc_chan_cfg = {
  .Channel      = 0,
  .Rank         = ADC_REGULAR_RANK_1,
  .SamplingTime = ADC_SAMPLETIME_2CYCLES_5,
  .SingleDiff   = ADC_SINGLE_ENDED,
  .OffsetNumber = ADC_OFFSET_NONE,
};

static void adc_cfg(ADC_HandleTypeDef *hadc, uint32_t channel)
{
  adc_chan_cfg.Channel = channel;
  HAL_ADC_ConfigChannel(hadc, &adc_chan_cfg);
}

static uint16_t adc_read_channel(ADC_HandleTypeDef *hadc, uint32_t channel)
{
  adc_cfg(hadc, channel);
  HAL_ADC_Start(hadc);
  HAL_ADC_PollForConversion(hadc, 10);
  return (uint16_t)HAL_ADC_GetValue(hadc);
}

// Shared tail for every scan flavor: print a throttled timing line and report
// any key whose value moved past the dead zone. Operates on g_hall_data in its
// [adc][sel*HALL_NUM_RANK + rank] layout. last_status_tick is per-flavor so the
// flavors each emit their own line once per second.
static void hall_finish(const char *label, uint32_t cycles, uint32_t *last_status_tick)
{
  uint16_t val_min = 0xFFFF, val_max = 0;
  for (int a = 0; a < HALL_NUM_ADC; a++)
    for (int s = 0; s < HALL_SLOTS_PER_ADC; s++)
    {
      uint16_t v = g_hall_data[a][s];
      if (v < val_min) val_min = v;
      if (v > val_max) val_max = v;
    }

  uint32_t now = HAL_GetTick();
  if (now - *last_status_tick >= 1000)
  {
    *last_status_tick = now;
    uint32_t cpu_freq = SystemCoreClock / 1000000u;
    uint32_t total_ns = cycles * 1000u / cpu_freq;
    // Free RAM = gap between the top of statics/heap (_end, from the linker)
    // and the live stack pointer. Snapshot at this call depth.
    extern uint32_t _end;
    uint32_t free_ram = __get_MSP() - (uint32_t)&_end;
    uint32_t free_kib_whole = free_ram / 1024u;
    uint32_t free_kib_frac  = (free_ram % 1024u) * 10u / 1024u;
    printf("%-16s: %6lu cycles (%5lu.%03lu us)  min=%4u max=%4u  free=%3lu.%lu KiB\r\n",
           label, (unsigned long)cycles,
           (unsigned long)(total_ns / 1000u), (unsigned long)(total_ns % 1000u),
           (unsigned)val_min, (unsigned)val_max,
           (unsigned long)free_kib_whole, (unsigned long)free_kib_frac);
  }

  for (int a = 0; a < HALL_NUM_ADC; a++)
    for (int s = 0; s < HALL_SLOTS_PER_ADC; s++)
    {
      uint16_t cur  = g_hall_data[a][s];
      uint16_t last = g_hall_last_reported[a][s];
      int delta = (int)cur - (int)last;
      if (delta < 0) delta = -delta;
      if (delta >= HALL_DEAD_ZONE)
      {
        printf(" > %-16s:  adc=%d sel=%d rank=%d: %4u -> %4u\r\n",
               label, a, s / HALL_NUM_RANK, s % HALL_NUM_RANK,
               (unsigned)last, (unsigned)cur);
        g_hall_last_reported[a][s] = cur;
      }
    }
}

static void cmd_hall_scan_naive_polling(void)
{
  static int initialized = 0;

  if (!initialized)
  {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
    HAL_GPIO_WritePin(HALL_NEN_GPIO_Port, HALL_NEN_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    initialized = 1;
  }

  uint32_t t_start = DWT->CYCCNT;
  for (int sel = 0; sel < HALL_NUM_SEL; sel++)
  {
    HAL_GPIO_WritePin(SEL0_GPIO_Port, SEL0_Pin, (sel & 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEL1_GPIO_Port, SEL1_Pin, (sel & 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    for (volatile int i = 0; i < MUX_LATENCY_CYCLES; i++) __NOP();
    g_hall_data[0][sel * HALL_NUM_RANK + 0] = adc_read_channel(&hadc1, ADC_CHANNEL_1);
    g_hall_data[0][sel * HALL_NUM_RANK + 1] = adc_read_channel(&hadc1, ADC_CHANNEL_2);
    g_hall_data[1][sel * HALL_NUM_RANK + 0] = adc_read_channel(&hadc2, ADC_CHANNEL_3);
    g_hall_data[1][sel * HALL_NUM_RANK + 1] = adc_read_channel(&hadc2, ADC_CHANNEL_4);
    g_hall_data[2][sel * HALL_NUM_RANK + 0] = adc_read_channel(&hadc3, ADC_CHANNEL_12);
    g_hall_data[2][sel * HALL_NUM_RANK + 1] = adc_read_channel(&hadc3, ADC_CHANNEL_1);
    g_hall_data[3][sel * HALL_NUM_RANK + 0] = adc_read_channel(&hadc4, ADC_CHANNEL_4);
    g_hall_data[3][sel * HALL_NUM_RANK + 1] = adc_read_channel(&hadc4, ADC_CHANNEL_5);
    g_hall_data[4][sel * HALL_NUM_RANK + 0] = adc_read_channel(&hadc5, ADC_CHANNEL_1);
    g_hall_data[4][sel * HALL_NUM_RANK + 1] = adc_read_channel(&hadc5, ADC_CHANNEL_2);
  }
  uint32_t cycles = DWT->CYCCNT - t_start;

  static uint32_t last_status_tick = 0;
  hall_finish("naive polling   ", cycles, &last_status_tick);
}

static void cmd_hall_scan_parallel(void)
{
  static int initialized = 0;

  if (!initialized)
  {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
    HAL_GPIO_WritePin(HALL_NEN_GPIO_Port, HALL_NEN_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    initialized = 1;
  }

  uint32_t t_start = DWT->CYCCNT;
  for (int sel = 0; sel < HALL_NUM_SEL; sel++)
  {
    HAL_GPIO_WritePin(SEL0_GPIO_Port, SEL0_Pin, (sel & 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEL1_GPIO_Port, SEL1_Pin, (sel & 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    for (volatile int i = 0; i < MUX_LATENCY_CYCLES; i++) __NOP();
    adc_cfg(&hadc1, ADC_CHANNEL_1);
    adc_cfg(&hadc2, ADC_CHANNEL_3);
    adc_cfg(&hadc3, ADC_CHANNEL_12);
    adc_cfg(&hadc4, ADC_CHANNEL_4);
    adc_cfg(&hadc5, ADC_CHANNEL_1);
    HAL_ADC_Start(&hadc1); HAL_ADC_Start(&hadc2); HAL_ADC_Start(&hadc3);
    HAL_ADC_Start(&hadc4); HAL_ADC_Start(&hadc5);
    HAL_ADC_PollForConversion(&hadc1, 10); HAL_ADC_PollForConversion(&hadc2, 10);
    HAL_ADC_PollForConversion(&hadc3, 10); HAL_ADC_PollForConversion(&hadc4, 10);
    HAL_ADC_PollForConversion(&hadc5, 10);
    g_hall_data[0][sel * HALL_NUM_RANK + 0] = (uint16_t)HAL_ADC_GetValue(&hadc1);
    g_hall_data[1][sel * HALL_NUM_RANK + 0] = (uint16_t)HAL_ADC_GetValue(&hadc2);
    g_hall_data[2][sel * HALL_NUM_RANK + 0] = (uint16_t)HAL_ADC_GetValue(&hadc3);
    g_hall_data[3][sel * HALL_NUM_RANK + 0] = (uint16_t)HAL_ADC_GetValue(&hadc4);
    g_hall_data[4][sel * HALL_NUM_RANK + 0] = (uint16_t)HAL_ADC_GetValue(&hadc5);
    adc_cfg(&hadc1, ADC_CHANNEL_2);
    adc_cfg(&hadc2, ADC_CHANNEL_4);
    adc_cfg(&hadc3, ADC_CHANNEL_1);
    adc_cfg(&hadc4, ADC_CHANNEL_5);
    adc_cfg(&hadc5, ADC_CHANNEL_2);
    HAL_ADC_Start(&hadc1); HAL_ADC_Start(&hadc2); HAL_ADC_Start(&hadc3);
    HAL_ADC_Start(&hadc4); HAL_ADC_Start(&hadc5);
    HAL_ADC_PollForConversion(&hadc1, 10); HAL_ADC_PollForConversion(&hadc2, 10);
    HAL_ADC_PollForConversion(&hadc3, 10); HAL_ADC_PollForConversion(&hadc4, 10);
    HAL_ADC_PollForConversion(&hadc5, 10);
    g_hall_data[0][sel * HALL_NUM_RANK + 1] = (uint16_t)HAL_ADC_GetValue(&hadc1);
    g_hall_data[1][sel * HALL_NUM_RANK + 1] = (uint16_t)HAL_ADC_GetValue(&hadc2);
    g_hall_data[2][sel * HALL_NUM_RANK + 1] = (uint16_t)HAL_ADC_GetValue(&hadc3);
    g_hall_data[3][sel * HALL_NUM_RANK + 1] = (uint16_t)HAL_ADC_GetValue(&hadc4);
    g_hall_data[4][sel * HALL_NUM_RANK + 1] = (uint16_t)HAL_ADC_GetValue(&hadc5);
  }
  uint32_t cycles = DWT->CYCCNT - t_start;

  static uint32_t last_status_tick = 0;
  hall_finish("parallel polling", cycles, &last_status_tick);
}

// ---- DMA scan flavor -------------------------------------------------------
// The .ioc only sets the board layer (pins -> analog, DMA1_Ch1..5 -> ADC1..5,
// circular periph->mem halfword, MINC, NVIC). It leaves each ADC in single-
// conversion mode (ScanConvMode disabled, NbrOfConversion=1, DMAContinuousReq
// disabled). We compensate here in code so this flavor can live side by side
// with the polling flavors, which need the ADCs back in single-conversion mode.

// Switch one ADC to a 2-rank regular scan feeding the (circular) DMA.
static void adc_set_scan2(ADC_HandleTypeDef *hadc, uint32_t chA, uint32_t chB)
{
  HAL_ADC_Stop(hadc);
  hadc->Init.ScanConvMode          = ADC_SCAN_ENABLE;
  hadc->Init.NbrOfConversion       = 2;
  hadc->Init.DMAContinuousRequests = ENABLE;   // match DMA_CIRCULAR from the MSP
  HAL_ADC_Init(hadc);

  ADC_ChannelConfTypeDef c = {0};
  c.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  c.SingleDiff   = ADC_SINGLE_ENDED;
  c.OffsetNumber = ADC_OFFSET_NONE;
  c.Rank = ADC_REGULAR_RANK_1; c.Channel = chA; HAL_ADC_ConfigChannel(hadc, &c);
  c.Rank = ADC_REGULAR_RANK_2; c.Channel = chB; HAL_ADC_ConfigChannel(hadc, &c);
}

// Restore the IOC default (single conversion) so the polling flavors still work.
static void adc_set_single(ADC_HandleTypeDef *hadc)
{
  HAL_ADC_Stop(hadc);
  hadc->Init.ScanConvMode          = ADC_SCAN_DISABLE;
  hadc->Init.NbrOfConversion       = 1;
  hadc->Init.DMAContinuousRequests = DISABLE;
  HAL_ADC_Init(hadc);
  // The polling flavors re-select rank-1's channel themselves via adc_cfg().
}

static void cmd_hall_scan_dma(void)
{
  static int initialized = 0;
  static ADC_HandleTypeDef *const adcs[5] = { &hadc1, &hadc2, &hadc3, &hadc4, &hadc5 };
  static const uint32_t chA[5] = { ADC_CHANNEL_1, ADC_CHANNEL_3, ADC_CHANNEL_12, ADC_CHANNEL_4, ADC_CHANNEL_1 };
  static const uint32_t chB[5] = { ADC_CHANNEL_2, ADC_CHANNEL_4, ADC_CHANNEL_1,  ADC_CHANNEL_5, ADC_CHANNEL_2 };

  if (!initialized)
  {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
    HAL_GPIO_WritePin(HALL_NEN_GPIO_Port, HALL_NEN_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    initialized = 1;
  }

  // Reconfigure for the DMA scan (untimed: not part of the measured work).
  for (int a = 0; a < 5; a++)
    adc_set_scan2(adcs[a], chA[a], chB[a]);

  uint32_t t_start = DWT->CYCCNT;
  for (int sel = 0; sel < HALL_NUM_SEL; sel++)
  {
    HAL_GPIO_WritePin(SEL0_GPIO_Port, SEL0_Pin, (sel & 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEL1_GPIO_Port, SEL1_Pin, (sel & 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    for (volatile int i = 0; i < MUX_LATENCY_CYCLES; i++) __NOP();

    if (sel == 0)
    {
      // Arm DMA for the whole sweep (8 = 4 sel x 2 ranks) straight into
      // g_hall_data, whose layout already matches the per-ADC buffer. Start_DMA
      // also issues the software trigger for sel 0. The circular buffer wraps
      // back to index 0 after exactly 8 transfers, i.e. at the end of the sweep.
      for (int a = 0; a < 5; a++)
        HAL_ADC_Start_DMA(adcs[a], (uint32_t *)g_hall_data[a], HALL_SLOTS_PER_ADC);
    }
    else
    {
      // ADCs are armed; just re-trigger. ContinuousConvMode is disabled, so the
      // ADC halts after each 2-rank sequence -> ADSTART is a clean per-sel gate.
      for (int a = 0; a < 5; a++)
        SET_BIT(adcs[a]->Instance->CR, ADC_CR_ADSTART);
    }

    // Wait for all 5 sequences (2 conversions each) to finish.
    for (int a = 0; a < 5; a++)
      while (READ_BIT(adcs[a]->Instance->CR, ADC_CR_ADSTART)) { }
  }
  uint32_t cycles = DWT->CYCCNT - t_start;

  for (int a = 0; a < 5; a++)
    HAL_ADC_Stop_DMA(adcs[a]);

  // DMA wrote straight into g_hall_data in its [adc][sel*rank + rank] layout,
  // so no de-interleave step is needed.

  // Leave the ADCs as the other flavors expect them.
  for (int a = 0; a < 5; a++)
    adc_set_single(adcs[a]);

  static uint32_t last_status_tick = 0;
  hall_finish("dma scan        ", cycles, &last_status_tick);
}

static uint8_t read_wing_id(void)
{
  uint8_t id = 0;
  if (HAL_GPIO_ReadPin(ID0_GPIO_Port, ID0_Pin) == GPIO_PIN_SET) id |= 1;
  if (HAL_GPIO_ReadPin(ID1_GPIO_Port, ID1_Pin) == GPIO_PIN_SET) id |= 2;
  if (HAL_GPIO_ReadPin(ID2_GPIO_Port, ID2_Pin) == GPIO_PIN_SET) id |= 4;
  return id;
}

typedef struct {
  GPIO_TypeDef *port;
  uint16_t pin;
  int remaining;
  uint32_t last_tick;
  int led_on;
} blink_state_t;

static volatile blink_state_t g_blink;

static void blink_tick(void)
{
  if (g_blink.remaining == 0)
    return;
  uint32_t now = HAL_GetTick();
  if (now - g_blink.last_tick < 200)
    return;
  g_blink.last_tick = now;
  if (g_blink.led_on)
  {
    HAL_GPIO_WritePin(g_blink.port, g_blink.pin, GPIO_PIN_RESET);
    g_blink.led_on = 0;
    g_blink.remaining--;
  }
  else
  {
    HAL_GPIO_WritePin(g_blink.port, g_blink.pin, GPIO_PIN_SET);
    g_blink.led_on = 1;
  }
}

int console_execute(int argc, const char * const *argv)
{
  if (argc == 0)
    return 0;
  if (strcmp(argv[0], "hello") == 0)
    printf("Hello from Wing %u!\r\n", read_wing_id());
  else if (strcmp(argv[0], "blink") == 0 && argc >= 2)
  {
    GPIO_TypeDef *port = NULL;
    uint16_t pin = 0;
    if (strcmp(argv[1], "fn0") == 0)       { port = LED_FN0_GPIO_Port; pin = LED_FN0_Pin; }
    else if (strcmp(argv[1], "ready") == 0) { port = READY_GPIO_Port;   pin = READY_Pin;   }
    else printf("Unknown LED: %s (fn0, ready)\r\n", argv[1]);
    if (port)
    {
      g_blink.port = port;
      g_blink.pin = pin;
      g_blink.remaining = 5;
      g_blink.last_tick = HAL_GetTick();
      g_blink.led_on = 0;
    }
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
  MX_ADC5_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  console_init(&huart1, USART1_IRQn);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint8_t fn0_prev = 0xFF;
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint8_t fn0 = HAL_GPIO_ReadPin(SW_FN0_GPIO_Port, SW_FN0_Pin) == GPIO_PIN_RESET ? 1 : 0;
    if (fn0 != fn0_prev)
    {
      fn0_prev = fn0;
      printf("FN0: %c\r\n", fn0 ? '1' : '0');
    }
    blink_tick();
    cmd_hall_scan_naive_polling();
    cmd_hall_scan_parallel();
    cmd_hall_scan_dma();
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

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
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
  sConfig.Channel = ADC_CHANNEL_3;
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
  sConfig.Channel = ADC_CHANNEL_4;
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
  * @brief ADC5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC5_Init(void)
{

  /* USER CODE BEGIN ADC5_Init 0 */

  /* USER CODE END ADC5_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC5_Init 1 */

  /* USER CODE END ADC5_Init 1 */

  /** Common config
  */
  hadc5.Instance = ADC5;
  hadc5.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc5.Init.Resolution = ADC_RESOLUTION_12B;
  hadc5.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc5.Init.GainCompensation = 0;
  hadc5.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc5.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc5.Init.LowPowerAutoWait = DISABLE;
  hadc5.Init.ContinuousConvMode = DISABLE;
  hadc5.Init.NbrOfConversion = 1;
  hadc5.Init.DiscontinuousConvMode = DISABLE;
  hadc5.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc5.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc5.Init.DMAContinuousRequests = DISABLE;
  hadc5.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc5.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc5) != HAL_OK)
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
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC5_Init 2 */

  /* USER CODE END ADC5_Init 2 */

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
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_HARD_OUTPUT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_ENABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_16BIT;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  huart1.Init.BaudRate = 115200;
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
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

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
  HAL_GPIO_WritePin(GPIOC, SEL0_Pin|SEL1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(HALL_NEN_GPIO_Port, HALL_NEN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED_FN0_Pin|READY_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : ID0_Pin */
  GPIO_InitStruct.Pin = ID0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ID0_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SEL0_Pin SEL1_Pin */
  GPIO_InitStruct.Pin = SEL0_Pin|SEL1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : ID1_Pin ID2_Pin */
  GPIO_InitStruct.Pin = ID1_Pin|ID2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : HALL_NEN_Pin */
  GPIO_InitStruct.Pin = HALL_NEN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(HALL_NEN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_FN0_Pin READY_Pin */
  GPIO_InitStruct.Pin = LED_FN0_Pin|READY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : SW_FN0_Pin */
  GPIO_InitStruct.Pin = SW_FN0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SW_FN0_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PGM_DETECT_Pin */
  GPIO_InitStruct.Pin = PGM_DETECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(PGM_DETECT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BOOT0_Pin */
  GPIO_InitStruct.Pin = BOOT0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOOT0_GPIO_Port, &GPIO_InitStruct);

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
