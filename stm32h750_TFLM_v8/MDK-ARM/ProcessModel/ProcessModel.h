/**
  ******************************************************************************
  * @file    ProcessModelOutput.h
  * @author  Борисов Руслан
  * @brief   Заголовочный файл модуля обработки TensorFlow Lite Micro
  * @date    15.03.2026
  *
  * @details Этот файл содержит объявления функций, структур и констант
  *          для работы с TensorFlow Lite Micro. Предоставляет полный функционал
  *          для инициализации модели, выполнения инференса и обработки результатов.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __PROCESS_MODEL_OUTPUT_H
#define __PROCESS_MODEL_OUTPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 *                               ВКЛЮЧАЕМЫЕ ФАЙЛЫ
 *============================================================================*/
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*============================================================================
 *                              КОНСТАНТЫ
 *============================================================================*/

/**
 * @defgroup ModelParams Параметры модели
 * @{
 */
#define MODEL_INPUT_WIDTH       32      /**< Ширина входного изображения */
#define MODEL_INPUT_HEIGHT      32      /**< Высота входного изображения */
#define MODEL_INPUT_CHANNELS    3       /**< Количество каналов (RGB) */
#define MODEL_INPUT_SIZE        (MODEL_INPUT_WIDTH * MODEL_INPUT_HEIGHT * MODEL_INPUT_CHANNELS) /**< Размер входного буфера */

#define TENSOR_ARENA_SIZE       (50 * 1024)  /**< Размер тензор арены (50KB) */
#define MAX_NUM_CLASSES         10            /**< Максимальное количество классов */
#define MAX_OPERATORS           64            /**< Максимальное количество операторов */
/** @} */

/**
 * @defgroup DisplayParams Параметры отображения
 * @{
 */
#define RESULT_BORDER_THICKNESS 2     /**< Толщина рамки на LCD */
#define RESULT_TEXT_AREA_HEIGHT 40    /**< Высота области текста */
/** @} */

/*============================================================================
 *                              ТИПЫ ДАННЫХ
 *============================================================================*/

/**
 * @enum   ModelState_t
 * @brief  Состояния модели TensorFlow Lite
 */
typedef enum {
    MODEL_STATE_UNINITIALIZED = 0, /**< Модель не инициализирована */
    MODEL_STATE_READY,              /**< Модель готова к работе */
    MODEL_STATE_ERROR,               /**< Ошибка инициализации/работы */
    MODEL_STATE_BUSY                  /**< Модель выполняет инференс */
} ModelState_t;

/**
 * @enum   TensorType_t
 * @brief  Типы выходных тензоров
 */
typedef enum {
    TENSOR_TYPE_UNKNOWN = 0,    /**< Неизвестный тип */
    TENSOR_TYPE_UINT8,           /**< Беззнаковый 8-битный */
    TENSOR_TYPE_INT8,            /**< Знаковый 8-битный */
    TENSOR_TYPE_FLOAT32          /**< 32-битный float */
} TensorType_t;

/**
 * @struct TensorInfo_t
 * @brief  Структура для хранения информации о тензоре
 */
typedef struct {
    TensorType_t type;           /**< Тип данных тензора */
    size_t bytes;                /**< Размер в байтах */
    size_t dimensions;           /**< Количество измерений */
    int32_t dims[4];             /**< Размеры по каждому измерению */
} TensorInfo_t;

/**
 * @struct InferenceResult_t
 * @brief  Результат последнего инференса
 */
typedef struct {
    int class_id;                           /**< ID распознанного класса */
    uint8_t confidence;                      /**< Уверенность (0-255) */
    uint8_t probabilities[MAX_NUM_CLASSES];  /**< Вероятности для всех классов */
    uint32_t inference_time_ms;               /**< Время выполнения инференса (мс) */
    uint32_t timestamp_ms;                    /**< Временная метка */
} InferenceResult_t;

/**
 * @struct PerformanceStats_t
 * @brief  Статистика производительности инференса
 */
typedef struct {
    uint32_t total_inference_time_ms; /**< Суммарное время всех инференсов (мс) */
    uint32_t max_inference_time_ms;   /**< Максимальное время инференса (мс) */
    uint32_t min_inference_time_ms;   /**< Минимальное время инференса (мс) */
    uint32_t last_inference_time_ms;  /**< Время последнего инференса (мс) */
    uint32_t avg_inference_time_ms;   /**< Среднее время инференса (мс) */
    uint32_t inference_count;         /**< Количество выполненных инференсов */
} PerformanceStats_t;

/*============================================================================
 *                      ПРОТОТИПЫ ФУНКЦИЙ
 *============================================================================*/

/**
 * @brief   Инициализация модуля TensorFlow Lite
 * 
 * @param   model_data      Указатель на данные модели (.tflite)
 * @param   model_size      Размер модели в байтах
 * @param   class_names     Массив строк с именами классов
 * @param   num_classes     Количество классов
 * @param   class_colors    Массив цветов для LCD (может быть NULL)
 * 
 * @retval  0   Успешная инициализация
 * @retval -1   Ошибка инициализации
 */
int ProcessModelOutput_Init(const unsigned char* model_data,
                           size_t model_size,
                           const char** class_names,
                           uint8_t num_classes,
                           const uint16_t* class_colors);

/**
 * @brief   Получение входного буфера модели
 * 
 * @return  Указатель на входной буфер или NULL если модель не инициализирована
 */
uint8_t* ProcessModelOutput_GetInputBuffer(void);

/**
 * @brief   Быстрая конвертация кадра с камеры во входной формат
 * 
 * @param   camera_buffer   Указатель на буфер камеры (RGB565)
 * @param   frame_width     Ширина кадра
 * @param   frame_height    Высота кадра
 */
void ProcessModelOutput_ConvertCameraFrame(uint16_t* camera_buffer,
                                          uint16_t frame_width,
                                          uint16_t frame_height);

/**
 * @brief   Запуск инференса на подготовленных данных
 * 
 * @retval  0   Успешное выполнение
 * @retval -1   Ошибка выполнения
 */
int ProcessModelOutput_RunInference(void);

/**
 * @brief   Получение последнего результата
 * 
 * @return  Указатель на структуру с результатом
 */
const InferenceResult_t* ProcessModelOutput_GetLastResult(void);

/**
 * @brief   Получение статистики производительности
 * 
 * @return  Указатель на структуру со статистикой
 */
const PerformanceStats_t* ProcessModelOutput_GetPerfStats(void);

/**
 * @brief   Сброс всей статистики
 */
void ProcessModelOutput_ResetStats(void);

/**
 * @brief   Получение состояния модели
 * 
 * @return  Текущее состояние модели
 */
ModelState_t ProcessModelOutput_GetState(void);

/**
 * @brief   Получение информации о входном тензоре
 * 
 * @return  Указатель на структуру с информацией
 */
const TensorInfo_t* ProcessModelOutput_GetInputInfo(void);

/**
 * @brief   Получение информации о выходном тензоре
 * 
 * @return  Указатель на структуру с информацией
 */
const TensorInfo_t* ProcessModelOutput_GetOutputInfo(void);

/**
 * @brief   Отображение результата на LCD
 * 
 * @param   result  Указатель на результат для отображения
 */
void ProcessModelOutput_DisplayResult(const InferenceResult_t* result);

/**
 * @brief   Анализ структуры модели (отладочная функция)
 */
void ProcessModelOutput_AnalyzeModel(void);

/**
 * @brief   Тестирование модели на тестовом паттерне
 * 
 * @retval  0   Успешное выполнение
 * @retval -1   Ошибка выполнения
 */
int ProcessModelOutput_TestWithPattern(void);

#ifdef __cplusplus
}
#endif

#endif /* __PROCESS_MODEL_OUTPUT_H */