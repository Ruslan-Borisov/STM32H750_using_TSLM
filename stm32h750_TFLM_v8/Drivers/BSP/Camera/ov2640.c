/**
  ******************************************************************************
  * @file    ov2640.c
  * @author  WeAct Studio (Based on OpenMV and OmniVision reference code)
  * @brief   Драйвер для камеры OV2640 (UXGA 2MP)
  * 
  * @attention
  * OV2640 - одна из самых популярных камер для встраиваемых систем.
  * Особенности:
  * - Максимальное разрешение: 1600x1200 (UXGA)
  * - Поддерживаемые форматы: RGB565, YUV422, JPEG (с аппаратным сжатием!)
  * - Встроенный DSP для обработки изображений
  * - Два банка регистров: сенсор (SENSOR) и DSP
  * 
  * @note Для STM32H750 важно:
  *       - JPEG режим позволяет экономить память и трафик (кадры меньше)
  *       - Для работы с JPEG нужен аппаратный JPEG кодек H750 (опционально)
  *       - При высоком FPS может потребоваться TIM1 для XCLK вместо MCO
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "ov2640.h"           // Заголовочный файл с определениями для OV2640
#include "ov2640_regs.h"      // Регистры камеры (адреса и битовые маски)

/* Private defines -----------------------------------------------------------*/
#define OV2640_XCLK_FREQUENCY       (20000000)  ///< Тактовая частота для камеры (20 МГц)
#define OV2640_NUM_ALLOWED_SIZES    (19)        ///< Количество поддерживаемых разрешений
#define NUM_BRIGHTNESS_LEVELS       (5)         ///< Количество уровней яркости (-2..+2)
#define NUM_CONTRAST_LEVELS         (5)         ///< Количество уровней контраста (-2..+2)
#define NUM_SATURATION_LEVELS       (5)         ///< Количество уровней насыщенности (-2..+2)
#define NUM_EFFECTS                 (9)         ///< Количество специальных эффектов
#define REGLENGTH                   8           ///< Длина регистра (не используется)

// Макрос для задержки (используем HAL_Delay из основного проекта)
#define ov2640_delay HAL_Delay

/* Private constants ---------------------------------------------------------*/

/**
  * @brief  Массив поддерживаемых разрешений для OV2640
  * 
  * Не все разрешения из общей таблицы поддерживаются OV2640.
  * Этот массив содержит только те, которые действительно работают.
  */
static const uint8_t allowed_sizes[OV2640_NUM_ALLOWED_SIZES] = {
        FRAMESIZE_CIF,      // 352x288
        FRAMESIZE_SIF,      // 352x240
        FRAMESIZE_QQVGA,    // 160x120
        FRAMESIZE_128X64,   // 128x64
        FRAMESIZE_QVGA,     // 320x240  - РЕКОМЕНДУЕТСЯ для начала
        FRAMESIZE_VGA,      // 640x480
        FRAMESIZE_HVGA,     // 480x320
        FRAMESIZE_WVGA2,    // 752x480
        FRAMESIZE_SVGA,     // 800x600
        FRAMESIZE_XGA,      // 1024x768
        FRAMESIZE_SXGA,     // 1280x1024
        FRAMESIZE_UXGA,     // 1600x1200 - максимум для OV2640
};

/**
  * @brief  Конфигурация для режима RGB565
  * 
  * RGB565 - 16 бит на пиксель (5 бит красный, 6 бит зеленый, 5 бит синий)
  * Используется для вывода на дисплей и обработки без сжатия.
  * 
  * Массив формата: {адрес регистра, значение}
  * Завершается {0,0}
  */
static const uint8_t rgb565_regs[][2] = {
    { BANK_SEL, BANK_SEL_DSP },           // Переключаемся на банк DSP
    { REG_RESET,  REG_RESET_DVP },        // Сброс DVP интерфейса
    { IMAGE_MODE, IMAGE_MODE_RGB565 },    // Устанавливаем режим RGB565
    { 0xD7,      0x03 },                  // Настройка формата вывода
    { 0xE1,      0x77 },                  // Настройка таймингов
    { REG_RESET, 0x00 },                   // Снимаем сброс
    {0, 0},                               // Терминатор
};

/**
  * @brief  Конфигурация для режима JPEG (сжатие)
  * 
  * JPEG - аппаратное сжатие изображения, кадры занимают меньше места
  * ОЧЕНЬ ПОЛЕЗНО для передачи по SPI или WiFi!
  * 
  * Массив формата: {адрес регистра, значение}
  * Завершается {0,0}
  */
static const uint8_t jpeg_regs[][2] = {
    {BANK_SEL,      BANK_SEL_DSP},                // Переключаемся на банк DSP
    {REG_RESET,     REG_RESET_DVP},                // Сброс DVP интерфейса
    {IMAGE_MODE,    IMAGE_MODE_JPEG_EN | IMAGE_MODE_RGB565}, // Включение JPEG
    {0xd7,          0x03},                         // Настройка формата
    {0xe1,          0x77},                         // Настройка таймингов
    {REG_RESET,     0x00},                          // Снимаем сброс
    {0,             0},                             // Терминатор
};

/**
  * @brief  Конфигурация для режима YUV422
  * 
  * YUV422 - формат с разделением яркости (Y) и цветности (UV)
  * Используется для некоторых алгоритмов обработки
  */
static const uint8_t yuyv_regs[][2] = {
    {BANK_SEL,      BANK_SEL_DSP},                  // Переключаемся на банк DSP
    {REG_RESET,     REG_RESET_DVP},                  // Сброс DVP интерфейса
    { IMAGE_MODE, IMAGE_MODE_YUV422 },               // Режим YUV422
    { 0xd7, 0x03 },                                   // Настройка формата
    { 0x33, 0xa0 },                                   // Настройка YUV
    { 0xe5, 0x1f },                                   // Доп. настройки
    { 0xe1, 0x67 },                                   // Тайминги
    { REG_RESET,  0x00 },                             // Снимаем сброс
    {0,             0},                               // Терминатор
};

/**
  * @brief  Таблица настроек яркости
  * 
  * Особый формат: через регистры BPADDR/BPDATA записываются значения
  * Первая строка - адреса регистров для записи
  * Последующие строки - значения для каждого уровня яркости
  */
static const uint8_t brightness_regs[NUM_BRIGHTNESS_LEVELS + 1][5] = {
    {BPADDR, BPDATA, BPADDR, BPDATA, BPDATA},        // Адреса для записи
    {0x00, 0x04, 0x09, 0x00, 0x00}, /* -2 */         // Уровень -2
    {0x00, 0x04, 0x09, 0x10, 0x00}, /* -1 */         // Уровень -1
    {0x00, 0x04, 0x09, 0x20, 0x00}, /*  0 */         // Нормальная яркость
    {0x00, 0x04, 0x09, 0x30, 0x00}, /* +1 */         // Уровень +1
    {0x00, 0x04, 0x09, 0x40, 0x00}, /* +2 */         // Уровень +2
};

/**
  * @brief  Таблица настроек контраста
  * 
  * Аналогично яркости, но больше регистров
  */
static const uint8_t contrast_regs[NUM_CONTRAST_LEVELS + 1][7] = {
    {BPADDR, BPDATA, BPADDR, BPDATA, BPDATA, BPDATA, BPDATA},
    {0x00, 0x04, 0x07, 0x20, 0x18, 0x34, 0x06}, /* -2 */
    {0x00, 0x04, 0x07, 0x20, 0x1c, 0x2a, 0x06}, /* -1 */
    {0x00, 0x04, 0x07, 0x20, 0x20, 0x20, 0x06}, /*  0 */
    {0x00, 0x04, 0x07, 0x20, 0x24, 0x16, 0x06}, /* +1 */
    {0x00, 0x04, 0x07, 0x20, 0x28, 0x0c, 0x06}, /* +2 */
};

/**
  * @brief  Таблица настроек насыщенности
  */
static const uint8_t saturation_regs[NUM_SATURATION_LEVELS + 1][5] = {
    {BPADDR, BPDATA, BPADDR, BPDATA, BPDATA},
    {0x00, 0x02, 0x03, 0x28, 0x28}, /* -2 */
    {0x00, 0x02, 0x03, 0x38, 0x38}, /* -1 */
    {0x00, 0x02, 0x03, 0x48, 0x48}, /*  0 */
    {0x00, 0x02, 0x03, 0x58, 0x58}, /* +1 */
    {0x00, 0x02, 0x03, 0x68, 0x68}, /* +2 */
};

/**
  * @brief  Таблица специальных эффектов
  * 
  * Каждый эффект задается тремя байтами (регистры 0x00, 0x05, 0x05+1)
  */
static const uint8_t OV2640_EFFECTS_TBL[NUM_EFFECTS][3]= {
    {0x00, 0x80, 0x80}, // Normal (обычный)
    {0x18, 0xA0, 0x40}, // Blueish (холодный, синеватый)
    {0x18, 0x40, 0xC0}, // Redish (теплый, красноватый)
    {0x18, 0x80, 0x80}, // Black and white (черно-белый)
    {0x18, 0x40, 0xA6}, // Sepia (сепия)
    {0x40, 0x80, 0x80}, // Negative (негатив)
    {0x18, 0x50, 0x50}, // Greenish (зеленоватый)
    {0x58, 0x80, 0x80}, // Black and white negative (негативный ч/б)
    {0x00, 0x80, 0x80}, // Normal (повтор для полноты)
};

/* Private function prototypes -----------------------------------------------*/

/**
  *===================================================================
  * @brief     Запись регистра OV2640 (обертка над Camera_WriteReg)
  *===================================================================
  * @param reg: Адрес регистра (8 бит)
  * @param data: Данные для записи (8 бит)
  * @retval 0 всегда (для совместимости)
  */
static uint8_t OV2640_WR_Reg(uint8_t reg, uint8_t data)
{
    Camera_WriteReg(&hcamera, reg, &data);
    return 0;
}

/**
  *===================================================================
  * @brief     Чтение регистра OV2640 (обертка над Camera_ReadReg)
  *===================================================================
  * @param reg: Адрес регистра (8 бит)
  * @retval Прочитанное значение (8 бит)
  */
static uint8_t OV2640_RD_Reg(uint8_t reg)
{
    uint8_t data;
    Camera_ReadReg(&hcamera, reg, &data);
    return data;
}

/**
  *===================================================================
  * @brief     Запись массива регистров (сенсор или DSP)
  *===================================================================
  * @param regs: Указатель на массив пар {адрес, значение}
  * @retval None
  * 
  * @note Массив должен заканчиваться {0,0} или {0xFF,0xFF}
  *       Запись идет через Camera_WriteReg (учитывает текущий банк)
  */
static void wrSensorRegs(const uint8_t (*regs)[2])
{
    for (int i = 0; regs[i][0]; i++) {
        Camera_WriteReg(&hcamera, regs[i][0], &regs[i][1]);
    }
}

/**
  *===================================================================
  * @brief     Сброс камеры (программный)
  *===================================================================
  * @param None
  * @retval 0 всегда
  * 
  * @note Последовательность:
  *       1. Переключиться на банк сенсора
  *       2. Установить бит сброса в COM7
  *       3. Подождать 5 мс
  *       4. Записать "медленные" регистры (начальная инициализация)
  *       5. Подождать 30 мс для стабилизации
  */
static int reset()
{
    ov2640_delay(100);                       // Ждем стабилизации питания
    
    // Reset all registers - программный сброс
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_SENSOR); // Переключаемся на банк сенсора
    OV2640_WR_Reg(COM7, COM7_SRST);           // Бит сброса (7 бит регистра COM7)
    
    ov2640_delay(5);                          // Ждем 5 мс
    
    // Записываем базовые регистры из таблицы ov2640_Slow_regs
    wrSensorRegs(ov2640_Slow_regs);
    
    ov2640_delay(30);                          // Ждем стабилизации после записи
    
    return 0;
}

/**
  *===================================================================
  * @brief     Установка специального эффекта
  *===================================================================
  * @param sde: Номер эффекта (0..NUM_EFFECTS-1)
  * @retval 0 при успехе, -1 при ошибке
  */
static int set_special_effect(uint8_t sde)
{
    if (sde >= NUM_EFFECTS) return -1;
    
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);           // Переключаемся на DSP
    OV2640_WR_Reg(BPADDR, 0x00);                      // Адрес для эффекта (старший)
    OV2640_WR_Reg(BPDATA, OV2640_EFFECTS_TBL[sde][0]); // Значение 1
    OV2640_WR_Reg(BPADDR, 0x05);                      // Адрес для эффекта (младший)
    OV2640_WR_Reg(BPDATA, OV2640_EFFECTS_TBL[sde][1]); // Значение 2
    OV2640_WR_Reg(BPDATA, OV2640_EFFECTS_TBL[sde][2]); // Значение 3 (автоинкремент)

    return 0;
}

/**
  *===================================================================
  * @brief     Управление экспозицией
  *===================================================================
  * @param exposure: 
  *          -1 - прочитать текущую экспозицию (возвращает значение)
  *          -2 - отключить автоэкспозицию и автоматическое усиление
  *          >0 - установить ручную экспозицию (значение)
  *          0 или другое - включить автоэкспозицию
  * @retval При запросе чтения (-1) возвращает текущую экспозицию,
  *         иначе 0
  * 
  * @note Экспозиция хранится в 3 регистрах (14 бит):
  *       REG45[5:0] - старшие 6 бит
  *       AEC[7:0]   - средние 8 бит
  *       REG04[1:0] - младшие 2 бита
  */
static int set_exposure(int exposure)
{
    int ret = 0;
    
    // Disable DSP - отключаем DSP для прямого доступа к сенсору
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
    OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_BYPAS); // DSP в обход (прямой доступ)

    OV2640_WR_Reg(BANK_SEL, BANK_SEL_SENSOR);    // Переключаемся на сенсор
    
    if (exposure == -1) {
        // Режим чтения текущей экспозиции
        uint16_t exp = (uint16_t)(OV2640_RD_Reg(REG45) & 0x3f) << 10; // Старшие биты
        exp |= (uint16_t)OV2640_RD_Reg(AEC) << 2;                     // Средние биты
        exp |= (uint16_t)OV2640_RD_Reg(REG04) & 0x03;                 // Младшие биты
        ret = (int)exp;
    }
    else if (exposure == -2) {
        // Отключаем автоэкспозицию и автоусиление
        OV2640_WR_Reg(COM8, COM8_SET(0));         // COM8 - управление AGC/AEC
    }
    else if (exposure > 0) {
        // Установка ручной экспозиции
        // Максимальная экспозиция зависит от разрешения
        int max_exp = (dvp_cam_resolution[hcamera.framesize][0] <= 800) ? 672 : 1248;
        if (exposure > max_exp) exposure = max_exp;
        
        OV2640_WR_Reg(COM8, COM8_SET(0));         // Отключаем авто
        OV2640_WR_Reg(REG45, (uint8_t)((exposure >> 10) & 0x3f));  // Старшие 6 бит
        OV2640_WR_Reg(AEC, (uint8_t)((exposure >> 2) & 0xff));      // Средние 8 бит
        OV2640_WR_Reg(REG04, (uint8_t)(exposure & 3));              // Младшие 2 бита
    }
    else {
        // Включаем автоэкспозицию и автоусиление
        OV2640_WR_Reg(COM8, COM8_SET(COM8_BNDF_EN | COM8_AGC_EN | COM8_AEC_EN));
    }

    // Enable DSP - включаем DSP обратно
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
    OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_EN);     // DSP включен
    
    return ret;
}

/**
  *===================================================================
  * @brief     Внутренняя функция установки разрешения
  *===================================================================
  * @param framesize: Желаемое разрешение
  * @retval None
  * 
  * @note Это основная функция настройки размера кадра.
  *       OV2640 имеет два основных набора регистров:
  *       - Для разрешений до SVGA (800x600) - ov2640_svga_regs
  *       - Для больших разрешений (UXGA) - ov2640_uxga_regs
  */
static void _set_framesize(uint8_t framesize)
{
    uint8_t cbar, qsreg, com7;

    // Сохраняем текущие настройки (цветные полосы и QS)
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_SENSOR);
    cbar = OV2640_RD_Reg(COM7) & COM7_COLOR_BAR;  // Сохраняем бит цветных полос
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
    qsreg = OV2640_RD_Reg(QS);                      // Сохраняем QS (качество JPEG)

    uint16_t w = dvp_cam_resolution[framesize][0];  // Ширина кадра
    uint16_t h = dvp_cam_resolution[framesize][1];  // Высота кадра
    const uint8_t (*regs)[2];

    // Выбираем набор регистров в зависимости от разрешения
    if (w <= dvp_cam_resolution[FRAMESIZE_SVGA][0]) regs = OV2640_svga_regs;
    else regs = OV2640_uxga_regs;

    // Disable DSP - отключаем DSP для конфигурации
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
    OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_BYPAS);

    // Write output width/height - устанавливаем выходные размеры
    // В OV2640 размеры хранятся как (реальный размер / 4)
    OV2640_WR_Reg(ZMOW, (w>>2)&0xFF);              // OUTW[7:0] (ширина/4)
    OV2640_WR_Reg(ZMOH, (h>>2)&0xFF);              // OUTH[7:0] (высота/4)
    OV2640_WR_Reg(ZMHH, ((h>>8)&0x04)|((w>>10)&0x03)); // Старшие биты

    // Set CLKRC - настройка тактирования (закомментировано)
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_SENSOR);
    //OV2640_WR_Reg(CLKRC, ov2640_sensor->night_mode);

    // Write DSP input registers - записываем выбранный набор
    wrSensorRegs(regs);

    // Restore color bar status - восстанавливаем цветные полосы
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_SENSOR);
    com7 = OV2640_RD_Reg(COM7) | cbar;
    OV2640_WR_Reg(COM7, com7);

    // Restore qs register - восстанавливаем QS
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
    OV2640_WR_Reg(QS, qsreg);

    // Enable DSP - включаем DSP обратно
    OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_EN);
}

/**
  *===================================================================
  * @brief     Установка формата пикселей (RGB565/JPEG/YUV)
  *===================================================================
  * @param pixformat: Желаемый формат
  * @retval 0 при успехе, -1 при ошибке
  * 
  * @note КРИТИЧЕСКИ ВАЖНО для STM32H750!
  *       - JPEG формат дает сжатые кадры (экономия памяти и трафика)
  *       - RGB565 нужен для вывода на дисплей
  *       - YUV422 используется некоторыми алгоритмами
  */
static int set_pixformat(pixformat_t pixformat)
{
    // Disable DSP - отключаем DSP для настройки
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
    OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_BYPAS);

    switch (pixformat) {
        case PIXFORMAT_RGB565:
            wrSensorRegs(rgb565_regs);      // RGB565
            break;
        case PIXFORMAT_JPEG:
            wrSensorRegs(jpeg_regs);        // JPEG (сжатый)
            break;
        case PIXFORMAT_YUV422:
            wrSensorRegs(yuyv_regs);         // YUV422
            break;
        default:
            // Enable DSP - при ошибке включаем DSP обратно
            OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
            OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_EN);
            return -1;
    }
    
    // Устанавливаем разрешение (вызовет включение DSP)
    _set_framesize(hcamera.framesize);
    
    return 0;
}

/**
  *===================================================================
  * @brief     Установка разрешения (внешняя функция)
  *===================================================================
  * @param framesize: Желаемое разрешение
  * @retval 0 при успехе, -1 если разрешение не поддерживается
  */
static int set_framesize(framesize_t framesize)
{
    int i = OV2640_NUM_ALLOWED_SIZES;
    
    // Проверяем, поддерживается ли запрошенное разрешение
    for (i=0; i<OV2640_NUM_ALLOWED_SIZES; i++) {
        if (allowed_sizes[i] == framesize) break;
    }
    if (i >= OV2640_NUM_ALLOWED_SIZES) {
        // LOGW - разрешение не поддерживается
        return -1;
    }

    hcamera.framesize = framesize;           // Сохраняем в глобальной структуре
    _set_framesize(framesize);                // Устанавливаем

    ov2640_delay(30);                         // Ждем стабилизации

    return 0;
}

/**
  *===================================================================
  * @brief     Проверка поддержки разрешения
  *===================================================================
  * @param framesize: Разрешение для проверки
  * @retval 0 если поддерживается, -1 если нет
  */
int ov2640_check_framesize(uint8_t framesize)
{
    int i = OV2640_NUM_ALLOWED_SIZES;
    for (i=0; i<OV2640_NUM_ALLOWED_SIZES; i++) {
        if (allowed_sizes[i] == framesize) break;
    }
    if (i >= OV2640_NUM_ALLOWED_SIZES) return -1;
    return 0;
}

/**
  *===================================================================
  * @brief     Установка контраста
  *===================================================================
  * @param level: Уровень контраста (-2..+2)
  * @retval 0 при успехе, -1 при ошибке
  */
static int set_contrast(int level)
{
    // Преобразуем -2..+2 в индекс 0..4
    level += (NUM_CONTRAST_LEVELS / 2) + 1;
    if (level < 0 || level > NUM_CONTRAST_LEVELS) {
        return -1;
    }

    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
    OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_BYPAS); // Отключаем DSP
    
    // Write contrast registers
    for (int i=0; i<sizeof(contrast_regs[0])/sizeof(contrast_regs[0][0]); i++) {
        OV2640_WR_Reg(contrast_regs[0][i], contrast_regs[level][i]);
    }
    
    OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_EN);    // Включаем DSP обратно

    return 0;
}

/**
  *===================================================================
  * @brief     Установка яркости
  *===================================================================
  * @param level: Уровень яркости (-2..+2)
  * @retval 0 при успехе, -1 при ошибке
  */
static int set_brightness(int level)
{
    // Преобразуем -2..+2 в индекс 0..4
    level += (NUM_BRIGHTNESS_LEVELS / 2) + 1;
    if ((level < 0) || (level > NUM_BRIGHTNESS_LEVELS)) return -1;

    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
    OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_BYPAS); // Отключаем DSP
    
    // Write brightness registers
    for (int i=0; i<sizeof(brightness_regs[0])/sizeof(brightness_regs[0][0]); i++) {
        OV2640_WR_Reg(brightness_regs[0][i], brightness_regs[level][i]);
    }
    
    OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_EN);    // Включаем DSP обратно

    return 0;
}

/**
  *===================================================================
  * @brief     Установка насыщенности
  *===================================================================
  * @param level: Уровень насыщенности (-2..+2)
  * @retval 0 при успехе, -1 при ошибке
  */
static int set_saturation(int level)
{
    // Преобразуем -2..+2 в индекс 0..4
    level += (NUM_SATURATION_LEVELS / 2) + 1;
    if ((level < 0) || (level > NUM_SATURATION_LEVELS)) return -1;

    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
    OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_BYPAS); // Отключаем DSP
    
    // Write saturation registers
    for (int i=0; i<sizeof(saturation_regs[0])/sizeof(saturation_regs[0][0]); i++) {
        OV2640_WR_Reg(saturation_regs[0][i], saturation_regs[level][i]);
    }
    
    OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_EN);    // Включаем DSP обратно

    return 0;
}

/**
  *===================================================================
  * @brief     Установка качества JPEG
  *===================================================================
  * @param qs: Значение качества (2..60, меньше = лучше качество)
  * @retval 0 при успехе, -1 при ошибке
  * 
  * @note QS (Quantization Scale) - масштаб квантования JPEG
  *       Меньшие значения = лучше качество, но больше размер
  *       Большие значения = хуже качество, но меньше размер
  */
static int set_quality(int qs)
{
    if ((qs < 2) || (qs > 60)) return -1;        // Проверка диапазона

    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
    OV2640_WR_Reg(QS, qs);                        // Записываем QS

    return 0;
}

/**
  *===================================================================
  * @brief     Включение/выключение тестовых цветных полос
  *===================================================================
  * @param enable: 1 - включить, 0 - выключить
  * @retval 0 всегда
  * 
  * @note Полезно для отладки - камера генерирует цветные полосы
  *       вместо реального изображения
  */
static int set_colorbar(int enable)
{
    uint8_t reg;
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_SENSOR);
    reg = OV2640_RD_Reg(COM7);                    // Читаем COM7

    if (enable) reg |= COM7_COLOR_BAR;             // Устанавливаем бит
    else reg &= ~COM7_COLOR_BAR;                   // Сбрасываем бит

    return OV2640_WR_Reg(COM7, reg);               // Записываем обратно
}

/**
  *===================================================================
  * @brief     Горизонтальное отражение (зеркало)
  *===================================================================
  * @param enable: 1 - нормально, 0 - отражено
  * @retval 0 всегда
  */
static int set_hmirror(int enable)
{
    uint8_t reg;
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_SENSOR);
    reg = OV2640_RD_Reg(REG04);                    // Читаем регистр 0x04

    if (!enable) {                                  // Если нужно отражение
        reg |= REG04_HFLIP_IMG;                     // Устанавливаем бит
    }
    else {
        reg &= ~REG04_HFLIP_IMG;                    // Сбрасываем бит
    }

    return OV2640_WR_Reg(REG04, reg);               // Записываем обратно
}

/**
  *===================================================================
  * @brief     Вертикальное отражение (переворот)
  *===================================================================
  * @param enable: 1 - нормально, 0 - перевернуто
  * @retval 0 всегда
  */
static int set_vflip(int enable)
{
    uint8_t reg;
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_SENSOR);
    reg = OV2640_RD_Reg(REG04);                    // Читаем регистр 0x04

    if (!enable) {                                  // Если нужно перевернуть
        reg |= REG04_VFLIP_IMG | REG04_VREF_EN;     // Устанавливаем биты
    }
    else {
        reg &= ~(REG04_VFLIP_IMG | REG04_VREF_EN);  // Сбрасываем биты
    }

    return OV2640_WR_Reg(REG04, reg);               // Записываем обратно
}

/**
  *===================================================================
  * @brief     Чтение ID камеры
  *===================================================================
  * @param manuf_id: Указатель для сохранения ID производителя
  * @param device_id: Указатель для сохранения ID устройства
  * @retval 0 всегда
  */
static int read_id(uint16_t *manuf_id, uint16_t *device_id)
{
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_SENSOR);
    *manuf_id = ((uint16_t)OV2640_RD_Reg(0x1C) << 8) | OV2640_RD_Reg(0x1D);
    *device_id = ((uint16_t)OV2640_RD_Reg(0x0A) << 8) | OV2640_RD_Reg(0x0B);
    return 0;
}

/**
  *===================================================================
  * @brief     Чтение регистра (обертка для совместимости)
  *===================================================================
  */
static int read_reg(uint16_t reg_addr)
{
    return (int)OV2640_RD_Reg((uint8_t) reg_addr);
}

/**
  *===================================================================
  * @brief     Запись регистра (обертка для совместимости)
  *===================================================================
  */
static int write_reg(uint16_t reg_addr, uint16_t reg_data)
{
    return (int)OV2640_WR_Reg((uint8_t)reg_addr, (uint8_t)reg_data);
}

/**
  * @brief  Таблица настроек освещения
  * 
  * Для разных условий освещения (солнечно, облачно, офис и т.д.)
  */
static const uint8_t OV2640_LIGHTMODE_TBL[5][3]=
{
    {0x5e, 0x41, 0x54}, // Auto - авто (не используется напрямую)
    {0x5e, 0x41, 0x54}, // Sunny - солнечно
    {0x52, 0x41, 0x66}, // Office - офис (искусственный свет)
    {0x65, 0x41, 0x4f}, // Cloudy - облачно
    {0x42, 0x3f, 0x71}, // Home - домашний (лампы накаливания)
};

/**
  *===================================================================
  * @brief     Установка режима освещения (баланс белого)
  *===================================================================
  * @param mode: 0 - авто, 1-4 - предустановки
  * @retval 0 при успехе, -1 при ошибке
  */
static int set_light_mode(uint8_t mode)
{
    if (mode > 4) return -1;

    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
    if (mode == 0) {
        OV2640_WR_Reg(0xc7, 0x00);                 // AWB on (авто баланс белого)
    }
    else {
        OV2640_WR_Reg(0xc7, 0x40);                 // AWB off (ручной режим)
        OV2640_WR_Reg(0xcc, OV2640_LIGHTMODE_TBL[mode][0]); // R усиление
        OV2640_WR_Reg(0xcd, OV2640_LIGHTMODE_TBL[mode][1]); // G усиление
        OV2640_WR_Reg(0xce, OV2640_LIGHTMODE_TBL[mode][2]); // B усиление
    }
    return 0;
}

/**
  *===================================================================
  * @brief     Установка ночного режима
  *===================================================================
  * @param enable: 1 - ночной режим, 0 - обычный
  * @retval 0 всегда
  * 
  * @note В ночном режиме камера работает на меньшей частоте,
  *       что позволяет снимать при слабом освещении (но меньше FPS)
  */
static int set_night_mode(int enable)
{
    // Disable DSP - отключаем DSP
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
    OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_BYPAS);
    
    // Set CLKRC - настраиваем тактирование сенсора
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_SENSOR);
    OV2640_WR_Reg(CLKRC, (enable) ? 0 : CLKRC_DOUBLE); // В ночном режиме - без удвоения
    
    // Enable DSP - включаем DSP обратно
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
    OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_EN);
    
    ov2640_delay(30);  // Ждем стабилизации
    return 0;
}

/**
  *===================================================================
  * @brief     Инициализация камеры OV2640
  *===================================================================
  * @param framesize: Начальное разрешение (рекомендуется FRAMESIZE_QVGA)
  * @retval 0 всегда
  * 
  * @note Это главная функция, вызываемая из camera.c после определения
  *       типа камеры. Устанавливает:
  *       1. Сброс камеры
  *       2. Начальное разрешение (из параметра)
  *       3. Формат пикселей (по умолчанию RGB565)
  *       4. Отключает отражение и переворот
  * 
  * @warning Для OV2640 на STM32H750 важно:
  *          - При проблемах с артефактами включите XCLK от TIM1
  *          - Для передачи по SPI/ESP32 используйте PIXFORMAT_JPEG
  *          - Для вывода на дисплей - PIXFORMAT_RGB565
  */
int ov2640_init(framesize_t framesize)
{
    reset();
    
    hcamera.framesize = framesize;
    hcamera.pixformat = PIXFORMAT_RGB565;
    
    // ПРИНУДИТЕЛЬНО УСТАНАВЛИВАЕМ RGB565
    OV2640_WR_Reg(BANK_SEL, BANK_SEL_DSP);
    OV2640_WR_Reg(R_BYPASS, R_BYPASS_DSP_BYPAS);
    wrSensorRegs(rgb565_regs);
    
    set_pixformat(hcamera.pixformat);
    set_hmirror(0);
    set_vflip(0);
    
    return 0;
}