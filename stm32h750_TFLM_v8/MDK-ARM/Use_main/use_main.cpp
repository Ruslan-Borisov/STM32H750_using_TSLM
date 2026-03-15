/**
  ******************************************************************************
  * @file    use_main.c
  * @author  Борисов Руслан
  * @brief   Основной файл приложения (упрощенная версия с модулем TFLite)
  * @date    15.03.2026
  ******************************************************************************
  */

/*============================================================================
 *                               ВКЛЮЧАЕМЫЕ ФАЙЛЫ
 *============================================================================*/
#include "main.h"
#include "dcmi.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"
#include "use_main.h"
#include "lcd.h"
#include "camera.h"
#include "uart_use.h"
#include "ProcessModel.h"
#include <string.h>
#include <stdio.h>

/* ============== МОДЕЛЬ ============== */
#include "model_uint8.h"

/*============================================================================
 *                              КОНСТАНТЫ
 *============================================================================*/
const char* class_names[] = {
    "EMPTY", "CIRCLE", "SQUARE", "TRIANGLE"
};

const uint16_t class_colors[] = {
    0xFFFF, 0x001F, 0xF800, 0x07E0
};

/*============================================================================
 *                              ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 *============================================================================*/
static uint32_t frame_count = 0;
static uint8_t first_inference_done = 0;

/*============================================================================
 *                         ИНИЦИАЛИЗАЦИЯ
 *============================================================================*/
void init_use(void) {
    Debug_Init();
    Debug_Printf("\r\n=======================================\r\n");
    Debug_Printf("      SYSTEM STARTING...\r\n");
    Debug_Printf("=======================================\r\n");
    
    /* Настройка прерываний */
    HAL_NVIC_SetPriority(DCMI_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DCMI_IRQn);
    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
    HAL_NVIC_SetPriority(SPI4_IRQn, 14, 0);
    HAL_NVIC_EnableIRQ(SPI4_IRQn);
    
    /* Инициализация LCD */
    Debug_Printf("Initializing LCD...\r\n");
    LCD_Test();
    ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, 0, LCD_WIDTH, LCD_HEIGHT, BLACK);
    
    /* Инициализация камеры */
    Debug_Printf("Initializing Camera...\r\n");
    Camera_Init_Device(&hi2c1, FRAMESIZE_QQVGA);
    
    if (hcamera.device_id == 0) {
        Debug_Printf("ERROR: Camera not found!\r\n");
        while(1) {
            HAL_Delay(100);
            LED_Blink(100, 900);
        }
    }
    
    Debug_Printf("Camera found, ID: 0x%04X\r\n", hcamera.device_id);
    
    /* Инициализация TFLite */
    Debug_Printf("\nInitializing TensorFlow Lite Micro...\r\n");
    if (ProcessModelOutput_Init(model_uint8, sizeof(model_uint8),
                               class_names, 4, class_colors) != 0) {
        Debug_Printf("ERROR: Failed to initialize TFLite!\r\n");
        while(1);
    }
    
    /* Запуск DMA */
    Debug_Printf("Starting camera DMA...\r\n");
    if (HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS,
                           (uint32_t)pic[0], FrameWidth * FrameHeight) != HAL_OK) {
        Debug_Printf("ERROR: DMA Start Failed!\r\n");
        while(1);
    }
    
    frame_count = 0;
    first_inference_done = 0;
    
    Debug_Printf("\n=======================================\r\n");
    Debug_Printf("      SYSTEM READY!\r\n");
    Debug_Printf("=======================================\r\n");
}

/*============================================================================
 *                      ОСНОВНОЙ ЦИКЛ
 *============================================================================*/
void use_while(void) {
    if (DCMI_FrameIsReady) {
        DCMI_FrameIsReady = 0;
        frame_count++;
        
        /* Вывод видео на LCD */
        ST7735_FillRGBRect(&st7735_pObj, 0, 0,
                          (uint8_t *)&pic[ready_buffer][LCD_START_ROW][0],
                          LCD_WIDTH, LCD_HEIGHT);
        
        /* Первый инференс после стабилизации */
        if (!first_inference_done && frame_count > 10) {
            Debug_Printf("\n=== FIRST INFERENCE ===\r\n");
            
            ProcessModelOutput_ConvertCameraFrame(
                (uint16_t*)pic[ready_buffer],
                FrameWidth, FrameHeight
            );
            
            if (ProcessModelOutput_RunInference() == 0) {
                const InferenceResult_t* result = ProcessModelOutput_GetLastResult();
                ProcessModelOutput_DisplayResult(result);
                first_inference_done = 1;
                
                const PerformanceStats_t* stats = ProcessModelOutput_GetPerfStats();
                Debug_Printf("First inference: %lu ms\r\n", stats->last_inference_time_ms);
            }
            
            Debug_Printf("=== FIRST INFERENCE COMPLETE ===\r\n\r\n");
        }
        
        /* Периодический вывод статистики */
        if (frame_count % 100 == 0 && first_inference_done) {
            const PerformanceStats_t* stats = ProcessModelOutput_GetPerfStats();
            Debug_Printf("[%lu] Avg: %lu ms, Min: %lu ms, Max: %lu ms\r\n",
                        frame_count,
                        stats->avg_inference_time_ms,
                        stats->min_inference_time_ms,
                        stats->max_inference_time_ms);
        }
    }
}