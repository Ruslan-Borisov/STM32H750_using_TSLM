/**
  ******************************************************************************
  * @file    use_main.h
  * @author  Борисов Руслан
  * @brief   Заголовочный файл для основной логики приложения
  * @date    15.03.2026
  *
  * @details Этот файл содержит объявления переменных и функций для основной
  *          логики приложения, включая работу с камерой, дисплеем и
  *          обработку видеопотока.
  *
  * @note    Все глобальные переменные размещены в AXI SRAM для быстрого доступа
  * @warning Для корректной работы необходимо предварительно инициализировать
  *          периферию через HAL
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USE_MAIN_H
#define __USE_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 *                               ВКЛЮЧАЕМЫЕ ФАЙЛЫ
 *============================================================================*/
#include "main.h"
#include "lcd.h"
#include "camera.h"
#include "i2c.h"
#include "dcmi.h"

/*============================================================================
 *                              КОНСТАНТЫ
 *============================================================================*/

/**
 * @defgroup DisplayConfig Конфигурация дисплея
 * @{
 */
#define TFT96 1  /**< Использование 0.96" дисплея */

#ifdef TFT96
/** @name Параметры для 0.96" дисплея (160x80) */
/** @{ */
#define FrameWidth          160   /**< Ширина кадра камеры */
#define FrameHeight         120   /**< Высота кадра камеры */
#define LCD_WIDTH           160   /**< Ширина дисплея */
#define LCD_HEIGHT          80    /**< Высота дисплея (используется часть кадра) */
#define LCD_START_ROW       20    /**< Смещение по вертикали для центрирования */
/** @} */
#else
#error "Please define TFT96 in your project settings"
#endif
/** @} */

/**
 * @defgroup BufferParams Параметры буферов
 * @{
 */
#define FRAME_BUFFER_SIZE   (FrameWidth * FrameHeight * 2)  /**< Размер буфера кадра (RGB565, 2 байта на пиксель) */
/** @} */

/**
 * @defgroup DebugFlags Флаги отладки
 * @{
 */
#define SHOW_DIAGNOSTICS    1   /**< Показывать диагностическую информацию */
#define SHOW_DETECTIONS     1   /**< Показывать рамки обнаруженных объектов */
#define SHOW_TRACKING       1   /**< Показывать информацию о трекинге */
/** @} */

/**
 * @defgroup DisplayColors Цвета для отображения (RGB565)
 * @{
 */
#define COLOR_DETECTION_RED     0xF800  /**< Красный - для рамок обнаружения */
#define COLOR_DETECTION_GREEN   0x07E0  /**< Зеленый - для рамок обнаружения */
#define COLOR_TRACKING_BLUE     0x001F  /**< Синий - для рамок трекинга */
#define COLOR_TRACKING_GREEN    0x07E0  /**< Зеленый - для рамок трекинга */
#define COLOR_TRACKING_YELLOW   0xFFE0  /**< Желтый - для рамок с низкой уверенностью */
#define COLOR_CENTER_CROSS      0xFFFF  /**< Белый - для перекрестия */
#define COLOR_DEVIATION_LINE    0xFFE0  /**< Желтый - для линии отклонения */
#define COLOR_INFO_PANEL        0x0000  /**< Черный - для панели информации */
#define COLOR_INFO_TEXT         0xFFFF  /**< Белый - для текста */
/** @} */

/**
 * @defgroup ConfidenceThresholds Пороги уверенности
 * @{
 */
#define CONFIDENCE_HIGH_THR     0.7f    /**< Порог высокой уверенности (70%) */
#define CONFIDENCE_MEDIUM_THR   0.4f    /**< Порог средней уверенности (40%) */
/** @} */

/**
 * @defgroup ButtonParams Параметры кнопки
 * @{
 */
#define BUTTON_DEBOUNCE         200     /**< Время антидребезга в мс */
/** @} */

/*============================================================================
 *                              ТИПЫ ДАННЫХ
 *============================================================================*/

/**
 * @enum   AppMode_t
 * @brief  Режимы работы приложения
 */
typedef enum {
    APP_MODE_NORMAL = 0,    /**< Обычный режим (видео + диагностика) */
    APP_MODE_DETECTION = 1, /**< Режим детекции (поиск объектов) */
    APP_MODE_TRACKING = 2,  /**< Режим трекинга (слежение за объектом) */
    APP_MODE_MENU = 3       /**< Режим меню (настройки) */
} AppMode_t;

/*============================================================================
 *                      ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 *============================================================================*/

/**
 * @brief   Двойной буфер для кадров камеры
 * @note    Размещается в SRAM для быстрого доступа DMA
 */
extern __attribute__((section(".sram"))) 
uint16_t pic[2][FrameHeight][FrameWidth];

/**
 * @brief   Буфер для конвертации данных перед выводом на LCD
 * @note    Размещается в SRAM
 */
extern __attribute__((section(".sram"))) 
uint16_t converted[LCD_WIDTH * 60];

/**
 * @name    Управление буферами камеры
 * @{
 */
extern volatile uint32_t active_buffer;  /**< Индекс активного буфера для записи */
extern volatile uint32_t ready_buffer;   /**< Индекс готового буфера для чтения */
/** @} */

/**
 * @name    Флаги и счетчики камеры
 * @{
 */
extern volatile uint32_t DCMI_FrameIsReady;  /**< Флаг готовности нового кадра */
extern volatile uint32_t Camera_FPS;          /**< Текущий FPS камеры */
extern volatile uint32_t LastFPSUpdate;       /**< Время последнего обновления FPS */
/** @} */

/**
 * @name    Диагностические счетчики
 * @{
 */
extern volatile uint32_t irq_counter;     /**< Счетчик прерываний */
extern volatile uint32_t display_counter; /**< Счетчик обновлений дисплея */
extern volatile uint32_t error_counter;   /**< Счетчик ошибок */
extern volatile uint32_t last_irq_value;  /**< Последнее значение IRQ */
/** @} */

/**
 * @name    Буферы для текста
 * @{
 */
extern uint8_t debug_text[50];  /**< Буфер для отладочного текста */
extern uint8_t line1[30];       /**< Буфер для строки 1 */
extern uint8_t line2[30];       /**< Буфер для строки 2 */
extern uint8_t line3[30];       /**< Буфер для строки 3 */
extern uint8_t line4[30];       /**< Буфер для строки 4 */
/** @} */

/**
 * @name    Счетчики циклов
 * @{
 */
extern uint32_t loop_counter;           /**< Счетчик циклов */
extern uint32_t last_display_update;    /**< Время последнего обновления дисплея */
extern uint32_t frame_counter;          /**< Счетчик кадров */
extern uint32_t inference_counter;      /**< Счетчик инференсов */
/** @} */

/**
 * @name    Текущий режим работы
 * @{
 */
extern AppMode_t current_mode;  /**< Текущий режим приложения */
/** @} */

/*============================================================================
 *                      ПРОТОТИПЫ ФУНКЦИЙ
 *============================================================================*/

/**
 * @brief   Управление светодиодом
 * @param   Hdelay  Время горения в миллисекундах
 * @param   Ldelay  Время паузы в миллисекундах
 * 
 * @note    Блокирующая функция с использованием HAL_Delay
 */
void LED_Blink(uint32_t Hdelay, uint32_t Ldelay);

/**
 * @brief   Вывод тестового паттерна на дисплей
 * @details Отображает цветные полосы для проверки дисплея
 */
void LCD_TestPattern(void);

/**
 * @brief   Обновление диагностической информации на дисплее
 * @details Выводит FPS, счетчики, состояние системы
 */
void UpdateDiagnosticDisplay(void);

/**
 * @brief   Рисование рамок вокруг обнаруженных объектов
 * @details Использует данные от модели детекции
 */
void DrawDetections(void);

/**
 * @brief   Рисование рамки вокруг отслеживаемого объекта
 * @details Использует данные от модели трекинга
 */
void DrawTracking(void);

/**
 * @brief   Рисование пунктирной рамки
 * @param   x       Координата X
 * @param   y       Координата Y
 * @param   w       Ширина рамки
 * @param   h       Высота рамки
 * @param   color   Цвет в формате RGB565
 */
void DrawDashedRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief   Обработка нажатия кнопки
 * @details Переключение режимов с антидребезгом
 */
void HandleButton(void);

/**
 * @brief   Вывод текста с фоном
 * @param   x       Координата X
 * @param   y       Координата Y
 * @param   text    Текст для вывода
 * @param   color   Цвет текста
 * @param   bg      Цвет фона
 */
void LCD_ShowTextOverlay(uint16_t x, uint16_t y, char* text, uint16_t color, uint16_t bg);

/**
 * @brief   Микросекундная задержка через NOP
 * @param   count   Количество циклов NOP
 * @note    Приблизительно 1 count = 1 такт процессора
 */
void delay_nop(uint32_t count);

/**
 * @brief   Инициализация всех компонентов приложения
 * @details Вызывает инициализацию UART, камеры, LCD и TFLite
 */
void init_use(void);

/**
 * @brief   Основной цикл обработки
 * @details Вызывается бесконечно из main.c, обрабатывает кадры с камеры
 *          и обновляет дисплей
 */
void use_while(void);

/**
 * @brief   Callback завершения кадра DCMI
 * @param   hdcmi   Указатель на структуру DCMI
 * @note    Вызывается из прерывания DMA
 */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi);

/**
 * @brief   Callback ошибки DCMI
 * @param   hdcmi   Указатель на структуру DCMI
 * @note    Вызывается при ошибках DMA/камеры
 */
void HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *hdcmi);

#ifdef __cplusplus
}
#endif

#endif /* __USE_MAIN_H */

/************************ (C) COPYRIGHT Борисов Руслан *****END OF FILE****/