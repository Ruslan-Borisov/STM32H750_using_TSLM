/**
  ******************************************************************************
  * @file    lcd.c
  * @author  Борисов Руслан
  * @brief   Драйвер LCD дисплея на базе ST7735
  * @date    15.03.2026
  *
  * @details Этот файл содержит реализацию низкоуровневого драйвера для
  *          дисплея ST7735 через SPI интерфейс. Поддерживает:
  *          - Инициализацию и тестирование дисплея
  *          - Управление подсветкой (аппаратный и программный ШИМ)
  *          - Вывод текста (шрифты 12x6 и 16x8)
  *          - Наложение текста на видео (прозрачный/непрозрачный режимы)
  *          - Ping-Pong буферизацию для работы с камерой
  *
  * @note    Предназначен для использования с микроконтроллерами STM32H7xx
  * @note    Поддерживает дисплеи 0.96" (160x80) и 1.8" (128x160)
  ******************************************************************************
  */

/*============================================================================
 *                               ВКЛЮЧАЕМЫЕ ФАЙЛЫ
 *============================================================================*/
#include "lcd.h"
#include "font.h"
#include "spi.h"
#include "tim.h"
#include "use_main.h"

/*============================================================================
 *                      ОПРЕДЕЛЕНИЯ ПИНОВ И АППАРАТУРЫ
 *============================================================================*/

/**
  * @name   Управление пинами LCD
  * @brief  Макросы для управления выводами дисплея
  * @{
  */

/** @brief Сброс дисплея (не используется в текущей реализации) */
#define LCD_RST_SET     
#define LCD_RST_RESET  

/** @brief LCD_RS (DC) - Команда/Данные */
#define LCD_RS_SET      HAL_GPIO_WritePin(LCD_WR_RS_GPIO_Port, LCD_WR_RS_Pin, GPIO_PIN_SET)   /**< PC4 - Режим данных */
#define LCD_RS_RESET    HAL_GPIO_WritePin(LCD_WR_RS_GPIO_Port, LCD_WR_RS_Pin, GPIO_PIN_RESET) /**< PC4 - Режим команды */

/** @brief LCD_CS - Выбор чипа */
#define LCD_CS_SET      HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET)   /**< Отключить */
#define LCD_CS_RESET    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET) /**< Включить */

/** @} */

/**
  * @name   Настройки SPI
  * @brief  Выбор SPI интерфейса для связи с дисплеем
  * @{
  */
#define SPI             spi4                    /**< Используем SPI4 */
#define SPI_Drv         (&hspi4)                /**< Указатель на структуру SPI */
#define delay_ms        HAL_Delay                /**< Функция задержки */
#define get_tick        HAL_GetTick              /**< Функция получения времени */
/** @} */

/**
  * @name   Управление подсветкой
  * @brief  Таймер для ШИМ регулировки яркости
  * @{
  */
#define LCD_Brightness_timer    &htim1          /**< Таймер для ШИМ */
#define LCD_Brightness_channel  TIM_CHANNEL_2   /**< Канал таймера */
/** @} */

/*============================================================================
 *                      СТАТИЧЕСКИЕ ПЕРЕМЕННЫЕ
 *============================================================================*/

/**
  * @brief  Установленное значение яркости (0-1000)
  */
static uint32_t LCD_LightSet;

/**
  * @brief  Флаг использования программного ШИМ
  */
static uint8_t IsLCD_SoftPWM = 0;

/*============================================================================
 *                      СТАТИЧЕСКИЕ ФУНКЦИИ (НИЗКОУРОВНЕВЫЕ)
 *============================================================================*/

static int32_t lcd_init(void);                    /**< Инициализация аппаратуры */
static int32_t lcd_gettick(void);                 /**< Получение текущего времени */
static int32_t lcd_writereg(uint8_t reg, uint8_t* pdata, uint32_t length);  /**< Запись в регистр */
static int32_t lcd_readreg(uint8_t reg, uint8_t* pdata);                    /**< Чтение регистра */
static int32_t lcd_senddata(uint8_t* pdata, uint32_t length);               /**< Отправка данных */
static int32_t lcd_recvdata(uint8_t* pdata, uint32_t length);               /**< Прием данных */

/*============================================================================
 *                      ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 *============================================================================*/

/**
  * @brief  Интерфейс ввода-вывода для драйвера ST7735
  * @note   Содержит указатели на низкоуровневые функции работы с аппаратурой
  */
ST7735_IO_t st7735_pIO = {
    lcd_init,       /**< Initialize */
    NULL,           /**< DeInitialize (не используется) */
    NULL,           /**< PowerControl (не используется) */
    lcd_writereg,   /**< WriteReg */
    lcd_readreg,    /**< ReadReg */
    lcd_senddata,   /**< SendData */
    lcd_recvdata,   /**< RecvData */
    lcd_gettick     /**< GetTick */
};

/**
  * @brief  Объект дисплея ST7735
  */
ST7735_Object_t st7735_pObj;

/**
  * @brief  ID дисплея
  */
uint32_t st7735_id;

/**
  * @brief  Цвет пера (текущий цвет рисования)
  * @note   По умолчанию белый (0xFFFF)
  */
uint16_t POINT_COLOR = 0xFFFF;

/**
  * @brief  Цвет фона
  * @note   По умолчанию черный
  */
uint16_t BACK_COLOR = BLACK;

/*============================================================================
 *                      ФУНКЦИИ ИНИЦИАЛИЗАЦИИ И ТЕСТИРОВАНИЯ
 *============================================================================*/

/**
  * @brief   Тестирование дисплея и отображение заставки
  * @details Выполняет полную инициализацию дисплея:
  *          1. Настройка конфигурации (ориентация, тип панели)
  *          2. Инициализация драйвера ST7735
  *          3. Отображение логотипа
  *          4. Ожидание нажатия кнопки с плавным включением подсветки
  *          5. Отображение информации о системе
  * 
  * @note    Должна вызываться один раз при старте системы
  * @note    Тип дисплея определяется макросом TFT96 или TFT18
  */
void LCD_Test(void)
{
    uint8_t text[20];
    
    /*========================================================================
     * 1. Настройка конфигурации в зависимости от типа дисплея
     *========================================================================*/
    #ifdef TFT96
    /* Дисплей 0.96 дюйма */
    ST7735Ctx.Orientation = ST7735_ORIENTATION_LANDSCAPE_ROT180;
    ST7735Ctx.Panel = HannStar_Panel;
    ST7735Ctx.Type = ST7735_0_9_inch_screen;
    #elif TFT18
    /* Дисплей 1.8 дюйма */
    ST7735Ctx.Orientation = ST7735_ORIENTATION_PORTRAIT;
    ST7735Ctx.Panel = BOE_Panel;
    ST7735Ctx.Type = ST7735_1_8a_inch_screen;
    #else
    #error "Unknown Screen"  /* Неизвестный тип дисплея */
    #endif
    
    /*========================================================================
     * 2. Инициализация драйвера
     *========================================================================*/
    ST7735_RegisterBusIO(&st7735_pObj, &st7735_pIO);
    ST7735_LCD_Driver.Init(&st7735_pObj, ST7735_FORMAT_RBG565, &ST7735Ctx);
    ST7735_LCD_Driver.ReadID(&st7735_pObj, &st7735_id);
    
    /* Выключаем подсветку */
    LCD_SetBrightness(0);
    
    /*========================================================================
     * 3. Отображение логотипа
     *========================================================================*/
    #ifdef TFT96
    extern unsigned char WeActStudiologo_160_80[];
    ST7735_LCD_Driver.DrawBitmap(&st7735_pObj, 0, 0, WeActStudiologo_160_80);
    #elif TFT18
    extern unsigned char WeActStudiologo_128_160[];
    ST7735_LCD_Driver.DrawBitmap(&st7735_pObj, 0, 0, WeActStudiologo_128_160);
    #endif
    
    /*========================================================================
     * 4. Ожидание нажатия кнопки с плавным включением подсветки
     *========================================================================*/
    uint32_t tick = get_tick();
    while (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) != GPIO_PIN_SET)
    {
        delay_ms(10);

        /* Первая секунда - плавное увеличение яркости */
        if (get_tick() - tick <= 1000)
        {
            LCD_SetBrightness((get_tick() - tick) * 100 / 1000);
        }
        /* С 1 до 3 секунд - отображение прогресс-бара */
        else if (get_tick() - tick <= 3000)
        {
            sprintf((char *)&text, "%03d", (get_tick() - tick - 1000) / 10);
            LCD_ShowString(ST7735Ctx.Width - 30, 1, ST7735Ctx.Width, 16, 16, text);
            ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, ST7735Ctx.Height - 3, 
                                      (get_tick() - tick - 1000) * ST7735Ctx.Width / 2000, 3, 0xFFFF);
        }
        /* После 3 секунд - выход */
        else if (get_tick() - tick > 3000)
            break;
    }
    
    /* Ожидание отпускания кнопки */
    while (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET)
    {
        delay_ms(10);
    }
    
    /* Плавное выключение подсветки */
    LCD_Light(0, 300);

    /*========================================================================
     * 5. Очистка экрана и отображение информации
     *========================================================================*/
    ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, 0, ST7735Ctx.Width, ST7735Ctx.Height, BLACK);

    /* Информация о проекте */
    sprintf((char *)&text, "WeAct Studio");
    LCD_ShowString(4, 4, ST7735Ctx.Width, 16, 16, text);
    
    /* Информация о микроконтроллере */
    sprintf((char *)&text, "STM32H7xx 0x%X", HAL_GetDEVID());
    LCD_ShowString(4, 22, ST7735Ctx.Width, 16, 16, text);
    
    /* Информация о дисплее */
    sprintf((char *)&text, "LCD ID:0x%X", st7735_id);
    LCD_ShowString(4, 40, ST7735Ctx.Width, 16, 16, text);

    /* Плавное включение подсветки */
    LCD_Light(600, 300);
}

/*============================================================================
 *                      ФУНКЦИИ УПРАВЛЕНИЯ ПОДСВЕТКОЙ
 *============================================================================*/

/**
  * @brief   Установка яркости подсветки
  * @param   Brightness  Яркость (0-1000)
  * 
  * @note    Если включен программный ШИМ, значение сохраняется для последующего использования
  * @note    При аппаратном ШИМ сразу устанавливает сравнение таймера
  */
void LCD_SetBrightness(uint32_t Brightness)
{
    LCD_LightSet = Brightness;
    if (!IsLCD_SoftPWM)
        __HAL_TIM_SetCompare(LCD_Brightness_timer, LCD_Brightness_channel, Brightness);
}

/**
  * @brief   Получение текущей яркости
  * @retval  uint32_t Текущая яркость (0-1000)
  */
uint32_t LCD_GetBrightness(void)
{
    if (IsLCD_SoftPWM)
        return LCD_LightSet;
    else
        return __HAL_TIM_GetCompare(LCD_Brightness_timer, LCD_Brightness_channel);
}

/**
  * @brief   Включение/выключение программного ШИМ
  * @param   enable  1 - включить, 0 - выключить
  * 
  * @note    При включении управление яркостью берет на себя прерывание таймера
  * @note    При выключении восстанавливается аппаратное управление
  */
void LCD_SoftPWMEnable(uint8_t enable)
{
    IsLCD_SoftPWM = enable;

    if (!enable)
        LCD_SetBrightness(LCD_LightSet);
}

/**
  * @brief   Проверка состояния программного ШИМ
  * @retval  uint8_t 1 - включен, 0 - выключен
  */
uint8_t LCD_SoftPWMIsEnable(void)
{
    return IsLCD_SoftPWM;
}

/**
  * @brief   Инициализация программного ШИМ
  * @details Настраивает TIM16 и пин PE10 для программного управления яркостью
  * 
  * @note    Использует таймер TIM16 с частотой 10KHz
  * @note    Пин PE10 используется как выход ШИМ
  */
void LCD_SoftPWMCtrlInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Включение тактирования порта E */
    __HAL_RCC_GPIOE_CLK_ENABLE();
    
    /* Настройка PE10 как выход */
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* Инициализация таймера TIM16 с частотой 10KHz */
    MX_TIM16_Init();
    HAL_TIM_Base_Start_IT(&htim16);

    /* Включение программного ШИМ */
    LCD_SoftPWMEnable(1);
}

/**
  * @brief   Деинициализация программного ШИМ
  * @details Останавливает таймер и деинициализирует пин
  */
void LCD_SoftPWMCtrlDeInit(void)
{
    HAL_TIM_Base_DeInit(&htim16);
    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_10);
}

/**
  * @brief   Обработчик программного ШИМ
  * @details Вызывается из прерывания таймера каждые 10ms.
  *          Управляет скважностью сигнала на пине PE10 в соответствии с LCD_LightSet.
  */
void LCD_SoftPWMCtrlRun(void)
{
    static uint32_t timecount;

    /* Тактирование 10ms */
    if (timecount > 1000)
        timecount = 0;
    else
        timecount += 10;

    /* Управление яркостью через скважность */
    if (timecount >= LCD_LightSet)
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
}

/**
  * @brief   Callback прерывания таймера
  * @param   htim  Указатель на структуру таймера
  * 
  * @note    Вызывает LCD_SoftPWMCtrlRun() для TIM16
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM16)
    {
        LCD_SoftPWMCtrlRun();
    }
}

/**
  * @brief   Плавное изменение яркости
  * @param   Brightness_Dis  Целевая яркость (0-1000)
  * @param   time            Время изменения в миллисекундах
  * 
  * @details Изменяет яркость по линейному закону от текущего значения до целевого
  *          за указанное время.
  */
void LCD_Light(uint32_t Brightness_Dis, uint32_t time)
{
    uint32_t Brightness_Now;
    uint32_t time_now;
    float temp1, temp2;
    float k, set;
    
    Brightness_Now = LCD_GetBrightness();
    time_now = 0;
    
    /* Если яркость уже равна целевой - выход */
    if (Brightness_Now == Brightness_Dis)
        return;
    
    /* Если время равно 0 - выход */
    if (time == time_now)
        return;
    
    /* Расчет коэффициента линейного изменения */
    temp1 = Brightness_Now;
    temp1 = temp1 - Brightness_Dis;
    temp2 = time_now;
    temp2 = temp2 - time;
    
    k = temp1 / temp2;
    
    /* Цикл изменения яркости */
    uint32_t tick = get_tick();
    while (1)
    {
        delay_ms(1);
        
        time_now = get_tick() - tick;
        temp2 = time_now - 0;
        set = temp2 * k + Brightness_Now;
        
        LCD_SetBrightness((uint32_t)set);
        
        if (time_now >= time)
            break;
    }
}

/*============================================================================
 *                      ФУНКЦИИ ВЫВОДА ТЕКСТА
 *============================================================================*/

/**
  * @brief   Отображение одного символа
  * @param   x     X координата начала символа
  * @param   y     Y координата начала символа
  * @param   num   Код символа (ASCII)
  * @param   size  Размер шрифта (12 или 16)
  * @param   mode  Режим отображения: 0 - с фоном, 1 - прозрачный
  * 
  * @note   Для size=12 используется шрифт asc2_1206 (12x6)
  * @note   Для size=16 используется шрифт asc2_1608 (16x8)
  */
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint8_t size, uint8_t mode)
{
    uint8_t temp, t1, t;
    uint16_t y0 = y;
    uint16_t x0 = x;
    uint16_t colortemp = POINT_COLOR;
    uint32_t h, w;
    
    /* Буфер для формирования изображения символа */
    uint16_t write[size][size == 12 ? 6 : 8];
    uint16_t count;
    
    /* Получение размеров экрана */
    ST7735_GetXSize(&st7735_pObj, &w);
    ST7735_GetYSize(&st7735_pObj, &h);
    
    /* Установка окна вывода */
    num = num - ' ';  /* Смещение в таблице шрифтов */
    count = 0;
    
    /*========================================================================
     * Режим с фоном (mode = 0)
     *========================================================================*/
    if (!mode)
    {
        for (t = 0; t < size; t++)
        {
            /* Выбор шрифта по размеру */
            if (size == 12)
                temp = asc2_1206[num][t];  /* Шрифт 12x6 */
            else
                temp = asc2_1608[num][t];  /* Шрифт 16x8 */
            
            for (t1 = 0; t1 < 8; t1++)
            {
                /* Определение цвета пикселя */
                if (temp & 0x80)
                    POINT_COLOR = (colortemp & 0xFF) << 8 | colortemp >> 8;
                else
                    POINT_COLOR = (BACK_COLOR & 0xFF) << 8 | BACK_COLOR >> 8;
                
                /* Запись в буфер */
                write[count][t / 2] = POINT_COLOR;
                count++;
                if (count >= size)
                    count = 0;
                
                temp <<= 1;
                y++;
                
                /* Проверка выхода за границы */
                if (y >= h)
                {
                    POINT_COLOR = colortemp;
                    return;
                }
                
                /* Переход на следующую строку */
                if ((y - y0) == size)
                {
                    y = y0;
                    x++;
                    if (x >= w)
                    {
                        POINT_COLOR = colortemp;
                        return;
                    }
                    break;
                }
            }
        }
    }
    /*========================================================================
     * Прозрачный режим (mode = 1)
     *========================================================================*/
    else
    {
        for (t = 0; t < size; t++)
        {
            /* Выбор шрифта по размеру */
            if (size == 12)
                temp = asc2_1206[num][t];
            else
                temp = asc2_1608[num][t];
            
            for (t1 = 0; t1 < 8; t1++)
            {
                /* Только установленные пиксели (остальные прозрачные) */
                if (temp & 0x80)
                {
                    write[count][t / 2] = (POINT_COLOR & 0xFF) << 8 | POINT_COLOR >> 8;
                }
                count++;
                if (count >= size)
                    count = 0;
                
                temp <<= 1;
                y++;
                
                /* Проверка выхода за границы */
                if (y >= h)
                {
                    POINT_COLOR = colortemp;
                    return;
                }
                
                /* Переход на следующую строку */
                if ((y - y0) == size)
                {
                    y = y0;
                    x++;
                    if (x >= w)
                    {
                        POINT_COLOR = colortemp;
                        return;
                    }
                    break;
                }
            }
        }
    }
    
    /* Вывод сформированного буфера на экран */
    ST7735_FillRGBRect(&st7735_pObj, x0, y0, (uint8_t *)&write, 
                       size == 12 ? 6 : 8, size);
    
    /* Восстановление цвета пера */
    POINT_COLOR = colortemp;
}

/**
  * @brief   Отображение строки текста
  * @param   x       Начальная X координата
  * @param   y       Начальная Y координата
  * @param   width   Максимальная ширина области (для переноса)
  * @param   height  Максимальная высота области
  * @param   size    Размер шрифта (12 или 16)
  * @param   p       Указатель на строку (завершается нулем)
  * 
  * @note   Автоматически переносит текст при достижении границы width
  */
void LCD_ShowString(uint16_t x, uint16_t y, uint16_t width, uint16_t height, 
                    uint8_t size, uint8_t *p)
{
    uint8_t x0 = x;
    width += x;
    height += y;
    
    /* Проход по строке до конца или до непечатного символа */
    while ((*p <= '~') && (*p >= ' '))
    {
        /* Переход на новую строку при достижении границы */
        if (x >= width)
        {
            x = x0;
            y += size;
        }
        
        /* Выход при достижении нижней границы */
        if (y >= height)
            break;
        
        /* Вывод символа */
        LCD_ShowChar(x, y, *p, size, 0);
        
        /* Переход к следующему символу */
        x += size / 2;
        p++;
    }
}

/*============================================================================
 *                      НИЗКОУРОВНЕВЫЕ ФУНКЦИИ SPI
 *============================================================================*/

/**
  * @brief   Инициализация аппаратуры
  * @retval  ST7735_OK при успехе
  */
static int32_t lcd_init(void)
{
    int32_t result = ST7735_OK;
    HAL_TIMEx_PWMN_Start(LCD_Brightness_timer, LCD_Brightness_channel);
    return result;
}

/**
  * @brief   Получение текущего времени в миллисекундах
  * @retval  uint32_t Время
  */
static int32_t lcd_gettick(void)
{
    return HAL_GetTick();
}

/**
  * @brief   Запись в регистр дисплея
  * @param   reg     Адрес регистра
  * @param   pdata   Указатель на данные
  * @param   length  Длина данных
  * @retval  0 при успехе, -1 при ошибке
  */
static int32_t lcd_writereg(uint8_t reg, uint8_t* pdata, uint32_t length)
{
    int32_t result;
    
    /* Выбор дисплея и режим команды */
    LCD_CS_RESET;
    LCD_RS_RESET;
    
    /* Отправка команды */
    result = HAL_SPI_Transmit(SPI_Drv, &reg, 1, 100);
    
    /* Переключение в режим данных */
    LCD_RS_SET;
    
    /* Отправка данных (если есть) */
    if (length > 0)
        result += HAL_SPI_Transmit(SPI_Drv, pdata, length, 500);
    
    /* Отключение дисплея */
    LCD_CS_SET;
    
    result = result > 0 ? -1 : 0;
    return result;
}

/**
  * @brief   Чтение регистра дисплея
  * @param   reg    Адрес регистра
  * @param   pdata  Указатель для сохранения данных
  * @retval  0 при успехе, -1 при ошибке
  */
static int32_t lcd_readreg(uint8_t reg, uint8_t* pdata)
{
    int32_t result;
    
    /* Выбор дисплея и режим команды */
    LCD_CS_RESET;
    LCD_RS_RESET;
    
    /* Отправка команды */
    result = HAL_SPI_Transmit(SPI_Drv, &reg, 1, 100);
    
    /* Переключение в режим данных */
    LCD_RS_SET;
    
    /* Прием данных */
    result += HAL_SPI_Receive(SPI_Drv, pdata, 1, 500);
    
    /* Отключение дисплея */
    LCD_CS_SET;
    
    result = result > 0 ? -1 : 0;
    return result;
}

/**
  * @brief   Отправка данных на дисплей
  * @param   pdata   Указатель на данные
  * @param   length  Длина данных
  * @retval  0 при успехе, -1 при ошибке
  */
static int32_t lcd_senddata(uint8_t* pdata, uint32_t length)
{
    int32_t result;
    
    /* Выбор дисплея */
    LCD_CS_RESET;
    
    /* Отправка данных (RS уже должен быть в режиме данных) */
    result = HAL_SPI_Transmit(SPI_Drv, pdata, length, 100);
    
    /* Отключение дисплея */
    LCD_CS_SET;
    
    result = result > 0 ? -1 : 0;
    return result;
}

/**
  * @brief   Прием данных с дисплея
  * @param   pdata   Указатель для сохранения данных
  * @param   length  Длина данных
  * @retval  0 при успехе, -1 при ошибке
  */
static int32_t lcd_recvdata(uint8_t* pdata, uint32_t length)
{
    int32_t result;
    
    /* Выбор дисплея */
    LCD_CS_RESET;
    
    /* Прием данных (RS уже должен быть в режиме данных) */
    result = HAL_SPI_Receive(SPI_Drv, pdata, length, 500);
    
    /* Отключение дисплея */
    LCD_CS_SET;
    
    result = result > 0 ? -1 : 0;
    return result;
}

/*============================================================================
 *                      ФУНКЦИИ ДЛЯ ВЫВОДА ПОВЕРХ ВИДЕО
 *============================================================================*/

/**
  * @brief   Вывод текста поверх видео с указанным цветом и фоном
  * @param   x       X координата начала текста
  * @param   y       Y координата начала текста
  * @param   str     Текст для вывода
  * @param   color   Цвет текста в формате RGB565
  * @param   bg      Цвет фона в формате RGB565
  * 
  * @note   Использует режим с фоном (mode=0)
  * @note   Для прозрачного фона используйте LCD_PrintOverlayTransparent
  */
void LCD_PrintOverlay(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg)
{
    /* Сохраняем текущие цвета */
    uint16_t temp_point = POINT_COLOR;
    uint16_t temp_back = BACK_COLOR;
    
    /* Устанавливаем новые цвета */
    POINT_COLOR = color;
    BACK_COLOR = bg;
    
    /* Выводим текст (mode = 0 - с фоном) */
    LCD_ShowString(x, y, 160, 80, 12, (uint8_t*)str);
    
    /* Восстанавливаем цвета */
    POINT_COLOR = temp_point;
    BACK_COLOR = temp_back;
}

/**
  * @brief   Вывод текста поверх видео (прозрачный фон)
  * @param   x       X координата начала текста
  * @param   y       Y координата начала текста
  * @param   str     Текст для вывода
  * @param   color   Цвет текста в формате RGB565
  * 
  * @note   Использует прозрачный режим (mode=1)
  * @note   Фон под текстом остается неизменным
  */
void LCD_PrintOverlayTransparent(uint16_t x, uint16_t y, char* str, uint16_t color)
{
    uint16_t temp_point = POINT_COLOR;
    uint8_t* p = (uint8_t*)str;
    uint8_t x0 = x;
    
    POINT_COLOR = color;
    
    /* Выводим каждый символ с mode = 1 (прозрачный) */
    while ((*p <= '~') && (*p >= ' '))
    {
        if (x >= 160)
        {
            x = x0;
            y += 12;
        }
        
        if (y >= 80)
            break;
            
        LCD_ShowChar(x, y, *p, 12, 1);  /* mode = 1 - прозрачный */
        x += 6;
        p++;
    }
    
    POINT_COLOR = temp_point;
}

/************************ (C) COPYRIGHT Борисов Руслан *****END OF FILE****/