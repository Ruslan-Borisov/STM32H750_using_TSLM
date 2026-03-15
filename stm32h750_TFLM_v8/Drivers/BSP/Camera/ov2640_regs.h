/**
  ******************************************************************************
  * @file    ov2640_regs.h
  * @author  OpenMV / MicroPython K210 project
  * @brief   Определения регистров камеры OV2640
  * 
  * @attention
  * Этот файл содержит все адреса регистров и битовые маски для OV2640.
  * Регистры разделены на два банка: DSP (обработка) и SENSOR (сенсор).
  * Переключение между банками осуществляется регистром BANK_SEL (0xFF).
  * 
  * @note Для STM32H750 критически важно понимать:
  *       - При работе с регистрами нужно правильно выбирать банк
  *       - Некоторые регистры доступны только в определенном банке
  *       - Битовые маски позволяют настраивать отдельные функции
  ******************************************************************************
  */

#ifndef __REG_REGS_H__
#define __REG_REGS_H__

#include <stdint.h>

/*============================================================================
 *                     DSP BANK REGISTERS (FF=0x00)
 *============================================================================*/

/**
  * @defgroup DSP_Registers Регистры банка DSP (обработки изображения)
  * @{
  */

#define QS                      0x44  ///< Quantization Scale (качество JPEG, 2-60)
#define HSIZE                   0x51  ///< Размер по горизонтали (H_SIZE[7:0] = реальный/4)
#define VSIZE                   0x52  ///< Размер по вертикали (V_SIZE[7:0] = реальный/4)
#define XOFFL                   0x53  ///< Смещение по X (младшие биты)
#define YOFFL                   0x54  ///< Смещение по Y (младшие биты)
#define VHYX                    0x55  ///< Старшие биты размеров и смещений
#define DPRP                    0x56  ///< Настройка DP (дефектных пикселей) и RP (восстановления)
#define TEST                    0x57  ///< Тестовый регистр (содержит старший бит H_SIZE[9])
#define ZMOW                    0x5A  ///< Zoom Out Width (выходная ширина/4)
#define ZMOH                    0x5B  ///< Zoom Out Height (выходная высота/4)
#define ZMHH                    0x5C  ///< Zoom Out старшие биты
#define BPADDR                  0x7C  ///< Адрес для доступа к DSP через почтовый ящик
#define BPDATA                  0x7D  ///< Данные для DSP (автоинкремент адреса)
#define SIZEL                   0x8C  ///< Младшие биты размеров
#define HSIZE8                  0xC0  ///< Старшие биты ширины (HSIZE[10:3])
#define VSIZE8                  0xC1  ///< Старшие биты высоты (VSIZE[10:3])
#define CTRL1                   0xC3  ///< Контрольный регистр 1
#define MS_SP                   0xF0  ///< 
#define SS_ID                   0xF7  ///< ID подсистемы
#define SS_CTRL                 0xF7  ///< Управление подсистемой
#define MC_AL                   0xFA  ///< Microcontroller Address Low
#define MC_AH                   0xFB  ///< Microcontroller Address High
#define MC_D                    0xFC  ///< Microcontroller Data
#define P_CMD                   0xFD  ///< Command register
#define P_STATUS                0xFE  ///< Status register

/**
  * @brief  CTRLI - Control Register I (0x50)
  * @note   Управление делителями и режимами
  */
#define CTRLI                   0x50
#define CTRLI_LP_DP             0x80  ///< Low Power mode for Dark Current compensation
#define CTRLI_ROUND             0x40  ///< Rounding mode

/**
  * @brief  CTRL0 - Control Register 0 (0xC2)
  * @note   Основные настройки формата вывода
  */
#define CTRL0                   0xC2
#define CTRL0_AEC_EN            0x80  ///< AEC (Auto Exposure) enable
#define CTRL0_AEC_SEL            0x40  ///< AEC select
#define CTRL0_STAT_SEL          0x20  ///< STAT select
#define CTRL0_VFIRST            0x10  ///< VFirst mode
#define CTRL0_YUV422            0x08  ///< YUV422 output format
#define CTRL0_YUV_EN            0x04  ///< YUV output enable
#define CTRL0_RGB_EN            0x02  ///< RGB output enable
#define CTRL0_RAW_EN            0x01  ///< RAW (Bayer) output enable

/**
  * @brief  CTRL2 - Control Register 2 (0x86)
  * @note   Настройки обработки изображения
  */
#define CTRL2                   0x86
#define CTRL2_DCW_EN            0x20  ///< DCW (Downsize) enable - уменьшение размера
#define CTRL2_SDE_EN            0x10  ///< SDE (Special Digital Effect) enable
#define CTRL2_UV_ADJ_EN         0x08  ///< UV adjustment enable (цветокоррекция)
#define CTRL2_UV_AVG_EN         0x04  ///< UV averaging enable (усреднение цветности)
#define CTRL2_CMX_EN            0x01  ///< Color Matrix enable (цветовая матрица)

/**
  * @brief  CTRL3 - Control Register 3 (0x87)
  * @note   Коррекция дефектных пикселей
  */
#define CTRL3                   0x87
#define CTRL3_BPC_EN            0x80  ///< BPC (Bad Pixel Correction) enable
#define CTRL3_WPC_EN            0x40  ///< WPC (White Pixel Correction) enable

/**
  * @brief  R_DVP_SP - DVP Speed Control (0xD3)
  */
#define R_DVP_SP                0xD3
#define R_DVP_SP_AUTO_MODE      0x80  ///< Automatic speed mode

/**
  * @brief  R_BYPASS - Bypass Control (0x05)
  * @note   Управление обходом DSP (для прямого доступа к сенсору)
  */
#define R_BYPASS                0x05
#define R_BYPASS_DSP_EN         0x00  ///< DSP включен (нормальный режим)
#define R_BYPASS_DSP_BYPAS      0x01  ///< DSP в обход (прямой доступ к сенсору)

/**
  * @brief  IMAGE_MODE - Image Mode (0xDA)
  * @note   Главный регистр для настройки формата выходных данных
  */
#define IMAGE_MODE              0xDA
#define IMAGE_MODE_Y8_DVP_EN    0x40  ///< Y8 format over DVP enable
#define IMAGE_MODE_JPEG_EN      0x10  ///< JPEG compression enable (аппаратное сжатие!)
#define IMAGE_MODE_YUV422       0x00  ///< YUV422 output
#define IMAGE_MODE_RAW10        0x04  ///< RAW10 output (10-bit Bayer)
#define IMAGE_MODE_RGB565       0x08  ///< RGB565 output (16-bit RGB)
#define IMAGE_MODE_HREF_VSYNC   0x02  ///< HREF vs VSYNC timing
#define IMAGE_MODE_LBYTE_FIRST  0x01  ///< Low byte first output
#define IMAGE_MODE_GET_FMT(x)   ((x)&0xC)  ///< Маска для получения формата

/**
  * @brief  REG_RESET - Reset Register (0xE0)
  * @note   Сброс различных блоков камеры
  */
#define REG_RESET               0xE0
#define REG_RESET_MICROC        0x40  ///< Reset microcontroller
#define REG_RESET_SCCB          0x20  ///< Reset SCCB interface
#define REG_RESET_JPEG          0x10  ///< Reset JPEG encoder
#define REG_RESET_DVP           0x04  ///< Reset DVP interface
#define REG_RESET_IPU           0x02  ///< Reset IPU
#define REG_RESET_CIF           0x01  ///< Reset CIF

/**
  * @brief  MC_BIST - Microcontroller BIST (0xF9)
  */
#define MC_BIST                 0xF9
#define MC_BIST_RESET           0x80  ///< BIST reset
#define MC_BIST_BOOT_ROM_SEL    0x40  ///< Boot from ROM select
#define MC_BIST_12KB_SEL        0x20  ///< 12KB memory select
#define MC_BIST_12KB_MASK       0x30  ///< 12KB mask
#define MC_BIST_512KB_SEL       0x08  ///< 512KB memory select
#define MC_BIST_512KB_MASK      0x0C  ///< 512KB mask
#define MC_BIST_BUSY_BIT_R      0x02  ///< Busy bit read
#define MC_BIST_MC_RES_ONE_SH_W 0x02  ///< MC reset one shot write
#define MC_BIST_LAUNCH          0x01  ///< Launch BIST

/**
  * @brief  BANK_SEL - Bank Select (0xFF)
  * @note   Самый важный регистр! Переключает между DSP и SENSOR банками.
  */
#define BANK_SEL                0xFF
#define BANK_SEL_DSP            0x00  ///< Выбрать банк DSP (обработка)
#define BANK_SEL_SENSOR         0x01  ///< Выбрать банк сенсора (аналоговая часть)

/** @} */

/*============================================================================
 *                   SENSOR BANK REGISTERS (FF=0x01)
 *============================================================================*/

/**
  * @defgroup Sensor_Registers Регистры банка сенсора (аналоговая часть)
  * @{
  */

#define GAIN                0x00  ///< Gain control (усиление)
#define COM1                0x03  ///< Common register 1
#define REG_PID             0x0A  ///< Product ID (старший байт)
#define REG_VER             0x0B  ///< Product Version (младший байт)
#define COM4                0x0D  ///< Common register 4
#define AEC                 0x10  ///< AEC value (младшие 8 бит экспозиции)

/**
  * @brief  CLKRC - Clock Rate Control (0x11)
  * @note   Управление тактовой частотой сенсора
  */
#define CLKRC               0x11
#define CLKRC_DOUBLE        0x80  ///< Double clock (удвоение частоты)
#define CLKRC_2X_UXGA       (0x01 | CLKRC_DOUBLE)  ///< 2x для UXGA
#define CLKRC_2X_SVGA       CLKRC_DOUBLE            ///< 2x для SVGA
#define CLKRC_2X_CIF        CLKRC_DOUBLE            ///< 2x для CIF
#define CLKRC_DIVIDER_MASK  0x3F  ///< Маска делителя (младшие 6 бит)

#define COM10               0x15  ///< Common register 10
#define HSTART              0x17  ///< Horizontal start (начало строки)
#define HSTOP               0x18  ///< Horizontal stop (конец строки)
#define VSTART              0x19  ///< Vertical start (начало кадра)
#define VSTOP               0x1A  ///< Vertical stop (конец кадра)
#define MIDH                0x1C  ///< Manufacturer ID High (производитель старший)
#define MIDL                0x1D  ///< Manufacturer ID Low (производитель младший)
#define AEW                 0x24  ///< AEC/AGC upper limit
#define AEB                 0x25  ///< AEC/AGC lower limit
#define REG2A               0x2A
#define FRARL               0x2B  ///< Frame rate control low
#define ADDVSL              0x2D  ///< VSYNC low
#define ADDVSH              0x2E  ///< VSYNC high
#define YAVG                0x2F  ///< Y average
#define HSDY                0x30
#define HEDY                0x31
#define ARCOM2              0x34  ///< Array control 2
#define REG45               0x45  ///< Старшие 6 бит экспозиции
#define FLL                 0x46  ///< Frame length low
#define FLH                 0x47  ///< Frame length high
#define COM19               0x48
#define ZOOMS               0x49
#define COM22               0x4B
#define COM25               0x4E
#define BD50                0x4F  ///< Banding filter for 50Hz
#define BD60                0x50  ///< Banding filter for 60Hz
#define REG5D               0x5D
#define REG5E               0x5E
#define REG5F               0x5F
#define REG60               0x60
#define HISTO_LOW           0x61  ///< Histogram low threshold
#define HISTO_HIGH          0x62  ///< Histogram high threshold

/**
  * @brief  REG04 - Register 0x04
  * @note   Управление ориентацией изображения и опорными сигналами
  */
#define REG04               0x04
#define REG04_DEFAULT       0x28  ///< Значение по умолчанию
#define REG04_HFLIP_IMG     0x80  ///< Horizontal flip (зеркало по горизонтали)
#define REG04_VFLIP_IMG     0x40  ///< Vertical flip (переворот по вертикали)
#define REG04_VREF_EN       0x10  ///< VREF enable
#define REG04_HREF_EN       0x08  ///< HREF enable
#define REG04_SET(x)        (REG04_DEFAULT|x)  ///< Макрос для установки битов

#define REG08               0x08

/**
  * @brief  COM2 - Common register 2 (0x09)
  * @note   Управление выходными драйверами
  */
#define COM2                0x09
#define COM2_STDBY          0x10  ///< Standby mode
#define COM2_OUT_DRIVE_1x   0x00  ///< Output drive strength 1x
#define COM2_OUT_DRIVE_2x   0x01  ///< Output drive strength 2x (рекомендуется)
#define COM2_OUT_DRIVE_3x   0x02  ///< Output drive strength 3x
#define COM2_OUT_DRIVE_4x   0x03  ///< Output drive strength 4x

/**
  * @brief  COM3 - Common register 3 (0x0C)
  * @note   Управление Banding Filter (подавление мерцания от ламп)
  */
#define COM3                0x0C
#define COM3_DEFAULT        0x38  ///< Значение по умолчанию
#define COM3_BAND_50Hz      0x04  ///< 50Hz banding (для ламп 50 Гц)
#define COM3_BAND_60Hz      0x00  ///< 60Hz banding (для ламп 60 Гц)
#define COM3_BAND_AUTO      0x02  ///< Automatic banding
#define COM3_BAND_SET(x)    (COM3_DEFAULT|x)  ///< Установка banding

/**
  * @brief  COM7 - Common register 7 (0x12)
  * @note   ОЧЕНЬ ВАЖНЫЙ РЕГИСТР! Управление сбросом, разрешением, тестовыми режимами.
  */
#define COM7                0x12
#define COM7_SRST           0x80  ///< Software reset (программный сброс)
#define COM7_RES_UXGA       0x00  ///< UXGA resolution (1600x1200)
#define COM7_RES_SVGA       0x40  ///< SVGA resolution (800x600)
#define COM7_RES_CIF        0x20  ///< CIF resolution (352x288)
#define COM7_ZOOM_EN        0x04  ///< Enable Zoom (включение масштабирования)
#define COM7_COLOR_BAR      0x02  ///< Enable Color Bar Test (тестовые полосы)
#define COM7_GET_RES(x)     ((x)&0x70)  ///< Получить текущее разрешение

/**
  * @brief  COM8 - Common register 8 (0x13)
  * @note   Управление AGC (автоусиление) и AEC (автоэкспозиция)
  */
#define COM8                0x13
#define COM8_DEFAULT        0xC0  ///< Значение по умолчанию
#define COM8_BNDF_EN        0x20  ///< Enable Banding filter (подавление мерцания)
#define COM8_AGC_EN         0x04  ///< AGC enable (автоусиление)
#define COM8_AEC_EN         0x01  ///< AEC enable (автоэкспозиция)
#define COM8_SET(x)         (COM8_DEFAULT|x)  ///< Установка битов с сохранением умолчаний
#define COM8_SET_AEC(r,x)   (((r)&0xFE)|((x)&1))  ///< Установка AEC с маской

/**
  * @brief  COM9 - Common register 9 (0x14)
  * @note   Управление максимальным усилением AGC
  */
#define COM9                0x14
#define COM9_DEFAULT        0x08  ///< Значение по умолчанию
#define COM9_AGC_GAIN_2x    0x00  ///< AGC max gain: 2x
#define COM9_AGC_GAIN_4x    0x01  ///< AGC max gain: 4x
#define COM9_AGC_GAIN_8x    0x02  ///< AGC max gain: 8x
#define COM9_AGC_GAIN_16x   0x03  ///< AGC max gain: 16x
#define COM9_AGC_GAIN_32x   0x04  ///< AGC max gain: 32x
#define COM9_AGC_GAIN_64x   0x05  ///< AGC max gain: 64x
#define COM9_AGC_GAIN_128x  0x06  ///< AGC max gain: 128x
#define COM9_AGC_SET(x)     (COM9_DEFAULT|(x<<5))  ///< Установка усиления

#define CTRL1_AWB           0x08  ///< Enable AWB (авто баланс белого)

/**
  * @brief  VV - Register 0x26
  * @note   AGC threshold
  */
#define VV                  0x26
#define VV_AGC_TH_SET(h,l)  ((h<<4)|(l&0x0F))  ///< Установка порогов AGC

/**
  * @brief  REG32 - Register 0x32
  * @note   Дополнительная настройка разрешения
  */
#define REG32               0x32
#define REG32_UXGA          0x36  ///< Для UXGA
#define REG32_SVGA          0x09  ///< Для SVGA
#define REG32_CIF           0x00  ///< Для CIF

/** @} */

/*============================================================================
 *                         МАКРОСЫ ДЛЯ УСТАНОВКИ ЗНАЧЕНИЙ
 *============================================================================*/

/**
  * @defgroup Setting_Macros Макросы для установки значений в регистры
  * @{
  */

/**
  * @brief  Общий макрос для установки битовых полей
  * @param  x: исходное значение
  * @param  mask: маска поля
  * @param  rshift: сдвиг в исходном значении
  * @param  lshift: сдвиг в целевом регистре
  */
#define VAL_SET(x, mask, rshift, lshift) ((((x) >> rshift) & mask) << lshift)

/* Макросы для CTRLI (делители) */
#define CTRLI_V_DIV_SET(x)      VAL_SET(x, 0x3, 0, 3)  ///< Vertical divider
#define CTRLI_H_DIV_SET(x)      VAL_SET(x, 0x3, 0, 0)  ///< Horizontal divider

/* Макросы для SIZEL (младшие биты размеров) */
#define SIZEL_HSIZE8_11_SET(x)  VAL_SET(x, 0x1, 11, 6) ///< 11-й бит HSIZE
#define SIZEL_HSIZE8_SET(x)     VAL_SET(x, 0x7, 0, 3)  ///< HSIZE[2:0]
#define SIZEL_VSIZE8_SET(x)     VAL_SET(x, 0x7, 0, 0)  ///< VSIZE[2:0]

/* Макросы для HSIZE8/VSIZE8 (старшие биты размеров) */
#define HSIZE8_SET(x)           VAL_SET(x, 0xFF, 3, 0) ///< HSIZE[10:3]
#define VSIZE8_SET(x)           VAL_SET(x, 0xFF, 3, 0) ///< VSIZE[10:3]

/* Макросы для HSIZE/VSIZE (основные размеры) */
#define HSIZE_SET(x)            VAL_SET(x, 0xFF, 2, 0) ///< H_SIZE[7:0] = реальная ширина/4
#define VSIZE_SET(x)            VAL_SET(x, 0xFF, 2, 0) ///< V_SIZE[7:0] = реальная высота/4

/* Макросы для смещений */
#define XOFFL_SET(x)            VAL_SET(x, 0xFF, 0, 0) ///< Offset X[7:0]
#define YOFFL_SET(x)            VAL_SET(x, 0xFF, 0, 0) ///< Offset Y[7:0]

/* Макросы для VHYX (старшие биты) */
#define VHYX_VSIZE_SET(x)       VAL_SET(x, 0x1, (8+2), 7) ///< VSIZE[8]
#define VHYX_HSIZE_SET(x)       VAL_SET(x, 0x1, (8+2), 3) ///< HSIZE[8]
#define VHYX_YOFF_SET(x)        VAL_SET(x, 0x3, 8, 4)     ///< Offset Y[9:8]
#define VHYX_XOFF_SET(x)        VAL_SET(x, 0x3, 8, 0)     ///< Offset X[9:8]

/* Макросы для TEST */
#define TEST_HSIZE_SET(x)       VAL_SET(x, 0x1, (9+2), 7) ///< HSIZE[9]

/* Макросы для ZMOW/ZMOH (выходной размер после зума) */
#define ZMOW_OUTW_SET(x)        VAL_SET(x, 0xFF, 2, 0)    ///< Output width/4
#define ZMOH_OUTH_SET(x)        VAL_SET(x, 0xFF, 2, 0)    ///< Output height/4

/* Макросы для ZMHH (старшие биты выходного размера) */
#define ZMHH_ZSPEED_SET(x)      VAL_SET(x, 0x0F, 0, 4)    ///< Zoom speed
#define ZMHH_OUTH_SET(x)        VAL_SET(x, 0x1, (8+2), 2) ///< Output height[8]
#define ZMHH_OUTW_SET(x)        VAL_SET(x, 0x3, (8+2), 0) ///< Output width[9:8]

/** @} */

/*============================================================================
 *                         СТАНДАРТНЫЕ РАЗМЕРЫ
 *============================================================================*/

/**
  * @defgroup Standard_Sizes Стандартные размеры изображения
  * @{
  */

#define CIF_WIDTH                   (400)   ///< CIF ширина
#define CIF_HEIGHT                  (296)   ///< CIF высота
#define SVGA_WIDTH                  (800)   ///< SVGA ширина
#define SVGA_HEIGHT                 (600)   ///< SVGA высота
#define UXGA_WIDTH                  (1600)  ///< UXGA ширина
#define UXGA_HEIGHT                 (1200)  ///< UXGA высота

/* Для регистров используются значения со сдвигом */
#define SVGA_HSIZE                  (800)   ///< SVGA ширина для регистров
#define SVGA_VSIZE                  (600)   ///< SVGA высота для регистров
#define UXGA_HSIZE                  (1600)  ///< UXGA ширина для регистров
#define UXGA_VSIZE                  (1200)  ///< UXGA высота для регистров

/** @} */

/*============================================================================
 *                         ВНЕШНИЕ ТАБЛИЦЫ
 *============================================================================*/

/**
  * @defgroup External_Tables Внешние таблицы регистров
  * @{
  */

extern const uint8_t OV2640_Fast_regs[][2];   ///< Быстрая инициализация (из Linux)
extern const uint8_t ov2640_Slow_regs[][2];   ///< Медленная инициализация (базовая)
extern const uint8_t OV2640_svga_regs[][2];   ///< Для разрешений ? SVGA
extern const uint8_t OV2640_uxga_regs[][2];   ///< Для разрешений > SVGA (UXGA)

/** @} */

#endif //__REG_REGS_H__