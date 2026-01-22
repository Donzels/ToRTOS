/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
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
#include "dma.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define IS_ENABLE_STATIC_ALLOCATION_TEST    1
#define IS_ENABLE_DYNAMIC_ALLOCATION_TEST   (1 - IS_ENABLE_STATIC_ALLOCATION_TEST)

#define IS_ENABLE_SEMA_TEST     1
#define IS_ENABLE_MUTEX_TEST    0
#define IS_ENABLE_QUEUE_TEST    0

#define THREAD_STACK_SIZE       512

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#if (IS_ENABLE_SEMA_TEST)
#if (IS_ENABLE_STATIC_ALLOCATION_TEST)
t_ipc_t sema1;
t_ipc_t *sema1_handle = &sema1;
t_thread_t sema_send_thread_instance;
t_thread_t sema_recv_thread_instance;
t_uint8_t sema_send_thread_stack[THREAD_STACK_SIZE];
t_uint8_t sema_recv_thread_stack[THREAD_STACK_SIZE];
#else
t_ipc_t *sema1_handle;
t_thread_t *sema_send_thread_handle;
t_thread_t *sema_recv_thread_handle;
#endif /* IS_ENABLE_STATIC_ALLOCATION_TEST */
void sema_send_thread(void *arg);
void sema_recv_thread(void *arg);
static t_uint8_t test_arg[] = {0xDE,0xAD}; 
#endif
#if (IS_ENABLE_MUTEX_TEST)
#if (IS_ENABLE_STATIC_ALLOCATION_TEST)
t_ipc_t mutex1;
t_ipc_t *mutex1_handle = &mutex1;
t_thread_t mutex_high_get_thread_instance;
t_thread_t mid_occupy_thread_instance;
t_thread_t mutex_low_get_thread_instance;
t_uint8_t mutex_high_get_thread_stack[THREAD_STACK_SIZE];
t_uint8_t mid_occupy_thread_stack[THREAD_STACK_SIZE];
t_uint8_t mutex_low_get_thread_stack[THREAD_STACK_SIZE];
#else
t_ipc_t *mutex1_handle;
t_thread_t *mutex_high_get_thread_handle;
t_thread_t *mid_occupy_thread_handle;
t_thread_t *mutex_low_get_thread_handle;
#endif
void mutex_high_get_thread(void *arg);
void mid_occupy_thread(void *arg);
void mutex_low_get_thread(void *arg);
#endif
#if (IS_ENABLE_QUEUE_TEST)
#define TEST_QUEUE_LENGTH   7
typedef struct
{
    t_uint32_t  time;
    t_uint8_t   i;
    float       f;
}msg_test_t;

#if (IS_ENABLE_STATIC_ALLOCATION_TEST)
t_ipc_t queue1;
t_ipc_t *queue1_handle = &queue1;
t_uint8_t queue_pool[sizeof(msg_test_t) * TEST_QUEUE_LENGTH];
t_thread_t queue_send_thread_instance;
t_thread_t queue_recv_thread_instance;
t_uint8_t queue_send_thread_stack[THREAD_STACK_SIZE];
t_uint8_t queue_recv_thread_stack[THREAD_STACK_SIZE];
#else
t_ipc_t *queue1_handle;
t_thread_t *queue_send_thread_handle;
t_thread_t *queue_recv_thread_handle;
#endif /* #if (IS_ENABLE_STATIC_ALLOCATION_TEST) */
void queue_send_thread(void *arg);
void queue_recv_thread(void *arg);
#endif /* IS_ENABLE_QUEUE_TEST */

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
    MX_USART1_UART_Init();
    /* USER CODE BEGIN 2 */
    t_tortos_init();
#if (IS_ENABLE_SEMA_TEST)
#if (IS_ENABLE_STATIC_ALLOCATION_TEST)
    T_SEMA_CREATE_STATIC(2, 0, TO_IPC_FLAG_FIFO, &sema1);
    t_thread_create_static(sema_send_thread,
                  sema_send_thread_stack,
                  THREAD_STACK_SIZE,
                  12,
                  &test_arg[0],
                  500,
                  &sema_send_thread_instance);
    t_thread_startup(&sema_send_thread_instance);
    t_thread_create_static(sema_recv_thread,
                  sema_recv_thread_stack,
                  THREAD_STACK_SIZE,
                  11,
                  &test_arg[1],
                  500,
                  &sema_recv_thread_instance);
    t_thread_startup(&sema_recv_thread_instance);
#else
    T_SEMA_CREATE(2, 0, TO_IPC_FLAG_FIFO, &sema1_handle);    
    t_thread_create(sema_send_thread,
                  THREAD_STACK_SIZE,
                  12,
                  &test_arg[0],
                  500,
                  &sema_send_thread_handle);
    t_thread_startup(sema_send_thread_handle);    
    t_thread_create(sema_recv_thread,
                  THREAD_STACK_SIZE,
                  11,
                  &test_arg[1],
                  500,
                  &sema_recv_thread_handle);
    t_thread_startup(sema_recv_thread_handle);
#endif /* IS_ENABLE_STATIC_ALLOCATION_TEST */
#endif /* IS_ENABLE_SEMA_TEST */
#if (IS_ENABLE_MUTEX_TEST)
#if (IS_ENABLE_STATIC_ALLOCATION_TEST)
    T_MUTEX_RECURSIVE_CREATE_STATIC(TO_IPC_FLAG_FIFO, &mutex1);    
    t_thread_create_static(mutex_high_get_thread,
                  mutex_high_get_thread_stack,
                  THREAD_STACK_SIZE,
                  15,
                  NULL,
                  10,
                  &mutex_high_get_thread_instance);
    t_thread_startup(&mutex_high_get_thread_instance);
    t_thread_create_static(mid_occupy_thread,
                  mid_occupy_thread_stack,
                  THREAD_STACK_SIZE,
                  14,
                  NULL,
                  10,
                  &mid_occupy_thread_instance);
    t_thread_startup(&mid_occupy_thread_instance);
    t_thread_create_static(mutex_low_get_thread,
                  mutex_low_get_thread_stack,
                  THREAD_STACK_SIZE,
                  13,
                  NULL,
                  10,
                  &mutex_low_get_thread_instance);
    t_thread_startup(&mutex_low_get_thread_instance);
#else
    T_MUTEX_RECURSIVE_CREATE(TO_IPC_FLAG_FIFO, &mutex1_handle);    
    t_thread_create(mutex_high_get_thread,
                  THREAD_STACK_SIZE,
                  15,
                  NULL,
                  10,
                  &mutex_high_get_thread_handle);
    t_thread_startup(mutex_high_get_thread_handle);
    t_thread_create(mid_occupy_thread,
                  THREAD_STACK_SIZE,
                  14,
                  NULL,
                  10,
                  &mid_occupy_thread_handle);
    t_thread_startup(mid_occupy_thread_handle);
    t_thread_create(mutex_low_get_thread,
                  THREAD_STACK_SIZE,
                  13,
                  NULL,
                  10,
                  &mutex_low_get_thread_handle);
    t_thread_startup(mutex_low_get_thread_handle);    
#endif /* IS_ENABLE_STATIC_ALLOCATION_TEST */
#endif /* IS_ENABLE_MUTEX_TEST */
#if (IS_ENABLE_QUEUE_TEST)
#if (IS_ENABLE_STATIC_ALLOCATION_TEST)
    T_QUEUE_CREATE_STATIC(queue_pool, TEST_QUEUE_LENGTH, sizeof(msg_test_t), TO_IPC_FLAG_FIFO, &queue1);
    t_thread_create_static(queue_send_thread,
                  queue_send_thread_stack,
                  THREAD_STACK_SIZE,
                  12,
                  NULL,
                  500,
                  &queue_send_thread_instance);
    t_thread_startup(&queue_send_thread_instance);
    t_thread_create_static(queue_recv_thread,
                  queue_recv_thread_stack,
                  THREAD_STACK_SIZE,
                  11,
                  NULL,
                  500,
                  &queue_recv_thread_instance);
    t_thread_startup(&queue_recv_thread_instance);    
#else
    T_QUEUE_CREATE(TEST_QUEUE_LENGTH, sizeof(msg_test_t), TO_IPC_FLAG_FIFO, &queue1_handle);
    t_thread_create(queue_send_thread,
                  THREAD_STACK_SIZE,
                  12,
                  NULL,
                  500,
                  &queue_send_thread_handle);
    t_thread_startup(queue_send_thread_handle);
    t_thread_create(queue_recv_thread,
                  THREAD_STACK_SIZE,
                  11,
                  NULL,
                  500,
                  &queue_recv_thread_handle);
    t_thread_startup(queue_recv_thread_handle);   
#endif /* IS_ENABLE_STATIC_ALLOCATION_TEST */   
#endif /* IS_ENABLE_QUEUE_TEST */
    t_sched_start();
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        HAL_Delay(500);
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
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
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 12;
    RCC_OscInitStruct.PLL.PLLN = 96;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
    {
        Error_Handler();
    }
}
#if (IS_ENABLE_SEMA_TEST)
void sema_send_thread(void *arg)
{
    t_uint8_t i = *(t_uint8_t *)arg;
    while (1)
    {
        float f = 3.14;
        f *= 2;
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        t_printf("sema send, f=%f, arg=0x%x\n", f, i);
        t_mdelay(500);
        T_SEMA_RELEASE(sema1_handle);
        T_SEMA_RELEASE(sema1_handle);
    }
}

void sema_recv_thread(void *arg)
{
    t_uint8_t i = *(t_uint8_t *)arg;
    while (1)
    {
        T_SEMA_ACQUIRE(sema1_handle, TO_WAITING_FOREVER);
        float f = 6.14;
        f *= -3;
        t_printf("sema recv, f=%f, arg=0x%x\n", f, i);
    }
}
#endif
#if (IS_ENABLE_MUTEX_TEST)
/* USER CODE BEGIN 4 */
void mutex_high_get_thread(void *arg) /* High priority (waiting for mutex) */
{
    int phase = 0;
    t_uint8_t i = 0;
    while (1)
    {
        if (0 == phase)
        {
            t_mdelay(100); /* Let low priority thread acquire mutex first to create inversion */
            t_printf("HIGH : try take mutex\n");
            if (T_OK == T_MUTEX_RECURSIVE_ACQUIRE(mutex1_handle, TO_WAITING_FOREVER))
            {
                t_printf("HIGH : got mutex (after inheritance) i=%d\n", i);
                T_MUTEX_RECURSIVE_RELEASE(mutex1_handle);
                t_printf("HIGH : released mutex\n");
                phase = 1;
            }
        }
        else
        {
            /* Simple demonstration of repeated acquire/release in subsequent cycles */
            if (T_OK == T_MUTEX_RECURSIVE_ACQUIRE(mutex1_handle, TO_WAITING_FOREVER))
            {
                T_MUTEX_RECURSIVE_RELEASE(mutex1_handle);
            }
            t_mdelay(600);
        }
        i++;
        if (255 == i)
            i = 0;
        t_mdelay(50);
    }
}

void mid_occupy_thread(void *arg) /* Medium priority (create CPU interference) */
{
    t_uint8_t i = 0;
    while (1)
    {
        i++;
        if (0 == i % 50)
            t_printf("MED  : running i=%d\n", i);
        if (255 == i)
            i = 0;
        /* No mutex usage, purely occupy time slice */
        t_mdelay(40);
    }
}

void mutex_low_get_thread(void *arg) /* Low priority (acquire mutex first and hold for long time) */
{
    int once = 0;
    t_uint8_t base_prio_saved = 0;
    while (1)
    {
        if (0 == once)
        {
            if (T_OK == T_MUTEX_RECURSIVE_ACQUIRE(mutex1_handle, TO_WAITING_FOREVER))
            {
                base_prio_saved = t_current_thread->current_priority;
                t_printf("LOW  : took mutex, do long work (base prio=%d)\n",
                         base_prio_saved);

                /* Simulate long task split into segments, 
                    high priority will wait during this period to trigger priority inheritance */
                for (int seg = 0; seg < 5; seg++)
                {
                    t_mdelay(120); /* Hold mutex during each segment */
                    if (t_current_thread)
                    {
                        if (t_current_thread->current_priority != base_prio_saved)
                            t_printf("LOW  : inherited priority -> %d (seg=%d)\n",
                                     t_current_thread->current_priority, seg);
                    }
                }

                t_printf("LOW  : releasing mutex\n");
                T_MUTEX_RECURSIVE_RELEASE(mutex1_handle);
                t_printf("LOW  : released mutex (should drop back to prio=%d)\n",
                         base_prio_saved);
                once = 1;
            }
        }
        else
        {
            /* Occasionally acquire mutex again later to verify normal path outside recursion */
            if (T_OK == T_MUTEX_RECURSIVE_ACQUIRE(mutex1_handle, TO_WAITING_FOREVER))
            {
                t_mdelay(30);
                T_MUTEX_RECURSIVE_RELEASE(mutex1_handle);
            }
            t_mdelay(200);
        }
        t_mdelay(10);
    }
}
#endif
#if (IS_ENABLE_QUEUE_TEST)
void queue_send_thread(void *arg)
{
    t_uint8_t i = 1;
    t_uint8_t j = 0;
    while (1)
    {
        
        float f = -0.37;
        f *= i;
        i++;
        if(i>10)
            i = 1;
        msg_test_t msg_test = 
        {
            .time = t_tick_get(),
            .f = f,
            .i = i
        };
        T_QUEUE_SEND(queue1_handle, &msg_test, 0);
        t_printf("queue send, tick=%d, i=%d, f=%f\r\n", msg_test.time, msg_test.i, msg_test.f);
        j++;
        if(j>TEST_QUEUE_LENGTH-1)
        {
            msg_test.time = 111;
            msg_test.i = 66;
            msg_test.f = 7.77;
            T_QUEUE_SEND(queue1_handle, &msg_test, 500);/* send one more after queue is full */
            t_printf("send one more queue, tick=%d, i=%d, f=%f\r\n", msg_test.time, msg_test.i, msg_test.f);
            j=0;
            t_mdelay(500);
        }        
    }
}
void queue_recv_thread(void *arg)
{
    while (1)
    {
        msg_test_t msg_test;
        T_QUEUE_RECV(queue1_handle, &msg_test, TO_WAITING_FOREVER);
        t_printf("queue recv, tick=%d, i=%d, f=%f\r\n", msg_test.time, msg_test.i, msg_test.f);
    }
}
#endif
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
