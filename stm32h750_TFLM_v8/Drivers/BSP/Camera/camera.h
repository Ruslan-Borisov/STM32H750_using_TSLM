/**
  ******************************************************************************
  * @file    camera.h
  * @author  Борисов Руслан (адаптация для WeAct Studio)
  * @brief   Заголовочный файл универсального драйвера камер OmniVision
  * @date    15.03.2026
  *
  * @details Этот файл содержит все определения типов, констант и прототипов
  *          функций, необходимых для работы с камерами OmniVision
  *          (OV7670, OV2640, OV7725, OV5640). Предоставляет унифицированный
  *          интерфейс для конфигурации камеры через I2C (SCCB протокол).
  *
  * @attention
  * Для STM32H750 важно:
  * - Все функции работают через глобальную структуру Camera_HandleTypeDef
  * - I2C адреса камер должны быть правильными (0x42/0x60/0x78)
  * - Разрешения выбираются из перечисления framesize_t
  * - Буфер кадров должен находиться в AXI SRAM (0x24000000)
  *
  * @note   Основная камера для этого проекта - OV2640 с I2C адресом 0x60
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CAMERA_H
#define CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 *                               ВКЛЮЧАЕМЫЕ ФАЙЛЫ
 *============================================================================*/
#include "main.h"  /**< Подключаем основной заголовочный файл проекта (содержит HAL и типы) */

/*============================================================================
 *                              КОНСТАНТЫ
 *============================================================================*/

/**
  * @defgroup Camera_Constants Константы драйвера камеры
  * @{
  */

/**
  * @brief  Режимы генерации тактового сигнала XCLK для камеры
  * @note   Используется в функции Camera_XCLK_Set()
  */
#define XCLK_TIM       0  /**< XCLK генерируется таймером TIM1 (PWM режим) */
#define XCLK_MCO       1  /**< XCLK генерируется MCO1 (Microcontroller Clock Output) */

/**
  * @brief  I2C адреса камер OmniVision
  * @note   Адреса указаны для режима записи (8-битный адрес)
  *         Для чтения обычно используется адрес+1
  * @note   OV2640 - основная камера проекта (адрес 0x60)
  */
#define OV7670_ADDRESS 0x42  /**< I2C адрес камеры OV7670 (8 бит, запись) */
#define OV2640_ADDRESS 0x60  /**< I2C адрес камеры OV2640 (8 бит, запись) - ВАША КАМЕРА! */
#define OV7725_ADDRESS 0x42  /**< I2C адрес камеры OV7725 (8 бит, запись) */
#define OV5640_ADDRESS 0x78  /**< I2C адрес камеры OV5640 (8 бит, запись) */

/**
  * @brief  Коды возврата функций
  */
#define Camera_OK      0     /**< Операция выполнена успешно */
#define camera_ERROR   1     /**< Произошла ошибка (I2C таймаут, нет ответа и т.д.) */

/**
  * @brief  Макрос для задержки (использует HAL_Delay)
  */
#define Camera_delay HAL_Delay

/** @} */

/*============================================================================
 *                              ТИПЫ ДАННЫХ
 *============================================================================*/

/**
  * @defgroup Camera_Types Типы данных драйвера камеры
  * @{
  */

/**
  * @brief  Структура для записи списка регистров
  * @details Используется для передачи длинных последовательностей регистров
  *          в функции Camera_WriteRegList(). Массив таких структур должен
  *          заканчиваться терминатором (обычно {0xFF, 0xFF}).
  * 
  * @note   Формат: {адрес регистра, значение}
  * @note   Терминатор: {0xFF, 0xFF} или {0, 0}
  */
struct regval_t {
    uint8_t reg_addr;  /**< Адрес регистра (8 бит) */
    uint8_t value;     /**< Значение для записи (8 бит) */
};

/**
  * @brief  Форматы пикселей (выходные форматы камеры)
  * @details Определяет, в каком виде камера будет отдавать данные.
  *          Критически важно для дальнейшей обработки!
  * 
  * @note   Для работы с дисплеем обычно используется PIXFORMAT_RGB565
  * @note   Для машинного зрения часто используют PIXFORMAT_YUV422 или PIXFORMAT_GRAYSCALE
  */
typedef enum {
    PIXFORMAT_INVALID = 0,  /**< Недействительный формат */
    PIXFORMAT_RGB565,       /**< RGB565 (16 бит на пиксель) - для дисплея */
    PIXFORMAT_JPEG,         /**< JPEG сжатый - для передачи по SPI/WiFi */
    PIXFORMAT_YUV422,       /**< YUV422 (16 бит на пиксель) - для обработки */
    PIXFORMAT_GRAYSCALE,    /**< Монохромное изображение (8 бит) */
    PIXFORMAT_BAYER,        /**< Сырые данные с сенсора (Bayer pattern) */
} pixformat_t;

/**
  * @brief  Разрешения кадра (frame sizes)
  * @details Полный список возможных разрешений. Не все камеры поддерживают все
  *          разрешения! Для OV2640 поддерживаемые разрешения:
  *          - QQVGA (160x120)
  *          - QVGA  (320x240)  - РЕКОМЕНДУЕТСЯ
  *          - VGA   (640x480)
  *          - UXGA  (1600x1200) - максимум
  * 
  * @note   Звездочками (*) отмечены наиболее часто используемые:
  *         FRAMESIZE_QVGA  (320x240)  - хорошее качество, небольшой размер
  *         FRAMESIZE_VGA    (640x480)  - стандартное качество
  *         FRAMESIZE_UXGA  (1600x1200) - максимальное для OV2640
  *         FRAMESIZE_720P  (1280x720)  - HD Ready
  *         FRAMESIZE_1080P (1920x1080) - Full HD (только для OV5640)
  */
typedef enum {
    FRAMESIZE_INVALID = 0,   /**< Недействительное разрешение */
    
    /* C/SIF Resolutions - старые видеоформаты */
    FRAMESIZE_QQCIF,    /**< 88x72   - очень маленькое */
    FRAMESIZE_QCIF,     /**< 176x144 - для видео */
    FRAMESIZE_CIF,      /**< 352x288 - для видео */
    FRAMESIZE_QQSIF,    /**< 88x60   - еще меньше */
    FRAMESIZE_QSIF,     /**< 176x120 - для видео */
    FRAMESIZE_SIF,      /**< 352x240 - для видео */
    
    /* VGA Resolutions - основные форматы */
    FRAMESIZE_QQQQVGA,  /**< 40x30   - крошечное */
    FRAMESIZE_QQQVGA,   /**< 80x60   - очень маленькое */
    FRAMESIZE_QQVGA,    /**< 160x120 - маленькое */
    FRAMESIZE_QVGA,     /**< 320x240 - РЕКОМЕНДУЕТСЯ (хороший баланс) */
    FRAMESIZE_VGA,      /**< 640x480 - стандартное VGA */
    FRAMESIZE_HQQQVGA,  /**< 60x40   - горизонтальное маленькое */
    FRAMESIZE_HQQVGA,   /**< 120x80  - горизонтальное */
    FRAMESIZE_HQVGA,    /**< 240x160 - горизонтальное */
    FRAMESIZE_HVGA,     /**< 480x320 - горизонтальное VGA */
    
    /* FFT Resolutions - для спектрального анализа */
    FRAMESIZE_64X32,    /**< 64x32   - для FFT */
    FRAMESIZE_64X64,    /**< 64x64   - для FFT */
    FRAMESIZE_128X64,   /**< 128x64  - для FFT */
    FRAMESIZE_128X128,  /**< 128x128 - для FFT */
    
    /* Other - другие форматы */
    FRAMESIZE_LCD,      /**< 128x160 - для маленьких дисплеев */
    FRAMESIZE_QQVGA2,   /**< 128x160 - альтернативное название */
    FRAMESIZE_WVGA,     /**< 720x480 - широкоформатный VGA */
    FRAMESIZE_WVGA2,    /**< 752x480 - чуть шире */
    FRAMESIZE_SVGA,     /**< 800x600 - Super VGA */
    FRAMESIZE_XGA,      /**< 1024x768 - Extended VGA */
    FRAMESIZE_SXGA,     /**< 1280x1024 - Super XGA */
    FRAMESIZE_UXGA,     /**< 1600x1200 - Ultra XGA (макс для OV2640) */
    FRAMESIZE_720P,     /**< 1280x720 - HD Ready (16:9) */
    FRAMESIZE_1080P,    /**< 1920x1080 - Full HD (только OV5640) */
    FRAMESIZE_960P,     /**< 1280x960 - 4:3 HD */
} framesize_t;

/**
  * @brief  Структура состояния камеры (главный handle)
  * @details Содержит всю информацию о текущем состоянии камеры:
  *          - Какой I2C интерфейс используется
  *          - Адрес камеры на шине I2C
  *          - Идентификаторы производителя и устройства
  *          - Текущие настройки (разрешение, формат)
  * 
  * @note   Глобальный экземпляр этой структуры - hcamera - объявлен ниже
  */
typedef struct {
    I2C_HandleTypeDef *hi2c;      /**< Указатель на структуру I2C (из CubeMX) */
    uint8_t addr;                  /**< I2C адрес камеры (для записи) */
    uint32_t timeout;              /**< Таймаут I2C операций в миллисекундах */
    uint16_t manuf_id;             /**< ID производителя (обычно 0x7FA2 для OmniVision) */
    uint16_t device_id;            /**< ID устройства (0x7673, 0x2642, 0x7721, 0x5640) */
    framesize_t framesize;         /**< Текущее разрешение */
    pixformat_t pixformat;          /**< Текущий формат пикселей */
} Camera_HandleTypeDef;

/** @} */

/*============================================================================
 *                      ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 *============================================================================*/

/**
  * @defgroup Camera_Global Глобальные переменные
  * @{
  */

/**
  * @brief   Глобальная структура состояния камеры
  * @details Определена в camera.c, доступна во всех файлах, включающих camera.h.
  *          Содержит текущее состояние подключенной камеры.
  * 
  * @note    После вызова Camera_Init_Device() проверьте hcamera.device_id
  *          Если device_id == 0 - камера не найдена
  */
extern Camera_HandleTypeDef hcamera;

/**
  * @brief   Таблица разрешений кадра
  * @details Двумерный массив, где каждая строка соответствует разрешению
  *          из перечисления framesize_t:
  *          - dvp_cam_resolution[разрешение][0] - ширина
  *          - dvp_cam_resolution[разрешение][1] - высота
  * 
  * @note    Определена в camera.c
  * @note    Пример: ширина = dvp_cam_resolution[FRAMESIZE_QVGA][0]
  */
extern const uint16_t dvp_cam_resolution[][2];

/** @} */

/*============================================================================
 *                      ПРОТОТИПЫ ФУНКЦИЙ
 *============================================================================*/

/**
  * @defgroup Camera_Functions Прототипы функций
  * @{
  */

/**
  * @brief   Запись одного байта в регистр камеры (8-битная адресация)
  * @param   hov     Указатель на структуру камеры
  * @param   regAddr Адрес регистра (8 бит)
  * @param   pData   Указатель на байт данных для записи
  * @retval  Camera_OK при успехе, camera_ERROR при ошибке
  */
int32_t Camera_WriteReg(Camera_HandleTypeDef *hov, uint8_t regAddr, const uint8_t *pData);

/**
  * @brief   Чтение одного байта из регистра камеры (8-битная адресация)
  * @param   hov     Указатель на структуру камеры
  * @param   regAddr Адрес регистра (8 бит)
  * @param   pData   Указатель для сохранения прочитанного байта
  * @retval  Camera_OK при успехе, camera_ERROR при ошибке
  */
int32_t Camera_ReadReg(Camera_HandleTypeDef *hov, uint8_t regAddr, uint8_t *pData);

/**
  * @brief   Запись в регистр с 16-битной адресацией (для OV5640)
  * @param   hov      Указатель на структуру камеры
  * @param   reg_addr Адрес регистра (16 бит)
  * @param   reg_data Байт данных для записи
  * @retval  Camera_OK при успехе, camera_ERROR при ошибке
  */
int32_t Camera_WriteRegb2(Camera_HandleTypeDef *hov, uint16_t reg_addr, uint8_t reg_data);

/**
  * @brief   Чтение из регистра с 16-битной адресацией (для OV5640)
  * @param   hov      Указатель на структуру камеры
  * @param   reg_addr Адрес регистра (16 бит)
  * @param   reg_data Указатель для сохранения прочитанного байта
  * @retval  Camera_OK при успехе, camera_ERROR при ошибке
  */
int32_t Camera_ReadRegb2(Camera_HandleTypeDef *hov, uint16_t reg_addr, uint8_t *reg_data);

/**
  * @brief   Запись списка регистров (массива конфигурации)
  * @param   hov       Указатель на структуру камеры
  * @param   reg_list  Указатель на массив структур regval_t
  * @retval  Camera_OK при успехе, код ошибки при первой неудачной записи
  * @note    Массив должен заканчиваться {0xFF, 0xFF} или {0, 0}
  */
int32_t Camera_WriteRegList(Camera_HandleTypeDef *hov, const struct regval_t *reg_list);

/**
  * @brief   Чтение идентификатора камеры (производитель и устройство)
  * @param   hov   Указатель на структуру камеры
  * @retval  0 всегда (результат сохраняется в полях структуры)
  * 
  * @note    После вызова:
  *          - hov->manuf_id содержит ID производителя (0x7FA2 для OmniVision)
  *          - hov->device_id содержит ID устройства
  */
int32_t Camera_read_id(Camera_HandleTypeDef *hov);

/**
  * @brief   Программный сброс камеры
  * @param   hov   Указатель на структуру камеры
  * @retval  None
  * 
  * @note    После сброса камера возвращается к настройкам по умолчанию
  * @note    Необходимо подождать ~100 мс для завершения сброса
  */
void Camera_Reset(Camera_HandleTypeDef *hov);

/**
  * @brief   Настройка источника тактового сигнала XCLK для камеры
  * @param   xclktype Тип источника (XCLK_TIM или XCLK_MCO)
  * @retval  None
  * 
  * @note    Критически важно для стабильной работы OV2640 на H750!
  * @note    XCLK_TIM использует TIM1_CH1, что конфликтует с подсветкой LCD
  * @note    XCLK_MCO использует MCO1, но может давать артефакты на высоких FPS
  */
void Camera_XCLK_Set(uint8_t xclktype);

/**
  * @brief   Инициализация камеры с автоматическим определением типа
  * @param   hi2c      Указатель на структуру I2C (какой I2C использовать)
  * @param   framesize Желаемое разрешение (например, FRAMESIZE_QVGA)
  * @retval  None
  * 
  * @note    После вызова проверьте hcamera.device_id:
  *          - Если 0 - камера не найдена
  *          - Если 0x2642/0x2643 - найдена OV2640 (ваша камера)
  *          - Если 0x7673 - найдена OV7670
  *          - Если 0x7721/0x7722 - найдена OV7725
  *          - Если 0x5640 - найдена OV5640
  */
void Camera_Init_Device(I2C_HandleTypeDef *hi2c, framesize_t framesize);

/** @} */

/*============================================================================
 *                      ВСПОМОГАТЕЛЬНЫЕ МАКРОСЫ
 *============================================================================*/

/**
  * @defgroup Camera_Macros Вспомогательные макросы
  * @{
  */

/** @brief Получение ширины текущего разрешения */
#define CAMERA_WIDTH  (dvp_cam_resolution[hcamera.framesize][0])

/** @brief Получение высоты текущего разрешения */
#define CAMERA_HEIGHT (dvp_cam_resolution[hcamera.framesize][1])

/** @brief Проверка, найдена ли камера */
#define CAMERA_IS_FOUND (hcamera.device_id != 0)

/** @brief Проверка, является ли камера OV2640 */
#define CAMERA_IS_OV2640 (hcamera.device_id == 0x2642 || hcamera.device_id == 0x2643)

/** @brief Проверка, является ли камера OV7670 */
#define CAMERA_IS_OV7670 (hcamera.device_id == 0x7673)

/** @brief Проверка, является ли камера OV7725 */
#define CAMERA_IS_OV7725 (hcamera.device_id == 0x7721 || hcamera.device_id == 0x7722)

/** @brief Проверка, является ли камера OV5640 */
#define CAMERA_IS_OV5640 (hcamera.device_id == 0x5640)

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CAMERA_H */

/************************ (C) COPYRIGHT Борисов Руслан *****END OF FILE****/