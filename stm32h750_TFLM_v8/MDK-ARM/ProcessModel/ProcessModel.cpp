/**
  ******************************************************************************
  * @file    ProcessModelOutput.cpp
  * @author  Борисов Руслан
  * @brief   Реализация модуля обработки TensorFlow Lite Micro
  * @date    15.03.2026
  *
  * @details Этот файл содержит реализацию функций для работы с TensorFlow Lite Micro:
  *          инициализация модели, конвертация изображений, выполнение инференса,
  *          обработка результатов и отображение на LCD.
  ******************************************************************************
  */

/*============================================================================
 *                               ВКЛЮЧАЕМЫЕ ФАЙЛЫ
 *============================================================================*/
#include "ProcessModel.h"
#include "lcd.h"
#include "uart_use.h"
#include "use_main.h"
#include "lcd.h"
#include <string.h>
#include <stdio.h>

/* ============== TENSORFLOW LITE MICRO ============== */
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"

/*============================================================================
 *                              ЛОКАЛЬНЫЕ КОНСТАНТЫ
 *============================================================================*/

/** @brief Стандартные имена классов (если не заданы пользователем) */
static const char* default_class_names[] = {
    "EMPTY",
    "CIRCLE",
    "SQUARE",
    "TRIANGLE"
};

/** @brief Стандартные цвета классов */
static const uint16_t default_class_colors[] = {
    0xFFFF,  // Белый
    0x001F,  // Синий
    0xF800,  // Красный
    0x07E0   // Зеленый
};

/*============================================================================
 *                         ЛОКАЛЬНЫЕ ТИПЫ ДАННЫХ
 *============================================================================*/
       // Буферы
    uint8_t model_input[MODEL_INPUT_SIZE] __attribute__((section(".sram")));
    uint8_t tensor_arena[TENSOR_ARENA_SIZE] __attribute__((aligned(16)));
/**
 * @brief   Внутренняя структура модуля (скрыта от пользователя)
 */
typedef struct {
    // TensorFlow Lite объекты
    tflite::MicroErrorReporter error_reporter;
    tflite::MicroMutableOpResolver<MAX_OPERATORS> resolver;
    const tflite::Model* model;
    tflite::MicroInterpreter* interpreter;
    


    
    // Карты масштабирования для быстрой конвертации
    uint8_t y_map[MODEL_INPUT_HEIGHT];
    uint8_t x_map[MODEL_INPUT_WIDTH];
    uint8_t maps_initialized;
    
    // Флаги инициализации
    uint8_t resolver_initialized;
    uint8_t interpreter_initialized;
    uint8_t model_analyzed;
    
    // Данные пользователя
    const char** class_names;
    uint8_t num_classes;
    const uint16_t* class_colors;
    
    // Результаты и статистика
    InferenceResult_t last_result;
    PerformanceStats_t perf_stats;
    int* class_counts;
} InternalContext_t;

/*============================================================================
 *                         ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
 *============================================================================*/

/** @brief Внутренний контекст модуля */
static InternalContext_t s_ctx;

/*============================================================================
 *                         ЛОКАЛЬНЫЕ ФУНКЦИИ
 *============================================================================*/

/**
 * @brief   Преобразование RGB565 в RGB888
 */
static void rgb565_to_rgb888(uint16_t rgb565, uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = ((rgb565 >> 8) & 0xF8);
    *g = ((rgb565 >> 3) & 0xFC);
    *b = ((rgb565 << 3) & 0xF8);
}

/**
 * @brief   Инициализация карт масштабирования
 */
static void init_scaling_maps(uint16_t frame_width, uint16_t frame_height) {
    if (s_ctx.maps_initialized) {
        return;
    }
    
    if (frame_width == 0 || frame_height == 0) {
        Debug_Printf("ERROR: Invalid frame dimensions!\r\n");
        return;
    }
    
    for (int i = 0; i < MODEL_INPUT_HEIGHT; i++) {
        s_ctx.y_map[i] = (i * frame_height) / MODEL_INPUT_HEIGHT;
        if (s_ctx.y_map[i] >= frame_height) {
            s_ctx.y_map[i] = frame_height - 1;
        }
    }
    
    for (int i = 0; i < MODEL_INPUT_WIDTH; i++) {
        s_ctx.x_map[i] = (i * frame_width) / MODEL_INPUT_WIDTH;
        if (s_ctx.x_map[i] >= frame_width) {
            s_ctx.x_map[i] = frame_width - 1;
        }
    }
    
    s_ctx.maps_initialized = 1;
    Debug_Printf("Scaling maps initialized: %dx%d -> %dx%d\r\n",
                frame_width, frame_height,
                MODEL_INPUT_WIDTH, MODEL_INPUT_HEIGHT);
}

/**
 * @brief   Обновление статистики производительности
 */
static void update_performance_stats(uint32_t inference_time_ms) {
    s_ctx.perf_stats.last_inference_time_ms = inference_time_ms;
    s_ctx.perf_stats.total_inference_time_ms += inference_time_ms;
    s_ctx.perf_stats.inference_count++;
    s_ctx.perf_stats.avg_inference_time_ms = 
        s_ctx.perf_stats.total_inference_time_ms / s_ctx.perf_stats.inference_count;
    
    if (inference_time_ms > s_ctx.perf_stats.max_inference_time_ms) {
        s_ctx.perf_stats.max_inference_time_ms = inference_time_ms;
    }
    if (inference_time_ms < s_ctx.perf_stats.min_inference_time_ms) {
        s_ctx.perf_stats.min_inference_time_ms = inference_time_ms;
    }
}

/**
 * @brief   Инициализация резолвера операций
 */
static bool init_op_resolver(void) {
    if (s_ctx.resolver_initialized) {
        return true;
    }
    
    Debug_Printf("\r\nInitializing Op Resolver...\r\n");
    
    TfLiteStatus status;
    bool all_success = true;
    
    status = s_ctx.resolver.AddConv2D();
    Debug_Printf("  AddConv2D(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddDepthwiseConv2D();
    Debug_Printf("  AddDepthwiseConv2D(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddMaxPool2D();
    Debug_Printf("  AddMaxPool2D(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddAveragePool2D();
    Debug_Printf("  AddAveragePool2D(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddFullyConnected();
    Debug_Printf("  AddFullyConnected(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddRelu();
    Debug_Printf("  AddRelu(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddSoftmax();
    Debug_Printf("  AddSoftmax(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddQuantize();
    Debug_Printf("  AddQuantize(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddDequantize();
    Debug_Printf("  AddDequantize(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddReshape();
    Debug_Printf("  AddReshape(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddShape();
    Debug_Printf("  AddShape(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddTranspose();
    Debug_Printf("  AddTranspose(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddConcatenation();
    Debug_Printf("  AddConcatenation(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddAdd();
    Debug_Printf("  AddAdd(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddMul();
    Debug_Printf("  AddMul(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddStridedSlice();
    Debug_Printf("  AddStridedSlice(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddPack();
    Debug_Printf("  AddPack(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddUnpack();
    Debug_Printf("  AddUnpack(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddSplit();
    Debug_Printf("  AddSplit(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    status = s_ctx.resolver.AddSlice();
    Debug_Printf("  AddSlice(): %s\r\n", status == kTfLiteOk ? "OK" : "FAILED");
    all_success &= (status == kTfLiteOk);
    
    if (all_success) {
        s_ctx.resolver_initialized = 1;
        Debug_Printf("Op Resolver initialized successfully\r\n");
    } else {
        Debug_Printf("ERROR: Op Resolver initialization failed\r\n");
    }
    
    return all_success;
}

/**
 * @brief   Анализ структуры модели
 */
static void analyze_model(void) {
    if (s_ctx.model_analyzed || !s_ctx.model) {
        return;
    }
    
    Debug_Printf("\r\n=== Model Analysis ===\r\n");
    Debug_Printf("Model version: %d\r\n", s_ctx.model->version());
    Debug_Printf("Schema version: %d\r\n", TFLITE_SCHEMA_VERSION);
    
    if (s_ctx.model->subgraphs() != NULL && s_ctx.model->subgraphs()->size() > 0) {
        auto* subgraph = (*s_ctx.model->subgraphs())[0];
        Debug_Printf("\r\nSubgraph 0:\r\n");
        
        if (subgraph->inputs() != NULL) {
            Debug_Printf("  Inputs: %zu\r\n", subgraph->inputs()->size());
        }
        
        if (subgraph->outputs() != NULL) {
            Debug_Printf("  Outputs: %zu\r\n", subgraph->outputs()->size());
        }
        
        if (subgraph->tensors() != NULL) {
            Debug_Printf("  Tensors: %zu\r\n", subgraph->tensors()->size());
        }
        
        if (subgraph->operators() != NULL) {
            Debug_Printf("  Operators: %zu\r\n", subgraph->operators()->size());
        }
    }
    
    Debug_Printf("=== End Model Analysis ===\r\n\r\n");
    s_ctx.model_analyzed = 1;
}

/**
 * @brief   Заполнение информации о тензоре
 */
static void fill_tensor_info(TfLiteTensor* tensor, TensorInfo_t* info) {
    if (!tensor || !info) {
        return;
    }
    
    switch (tensor->type) {
        case kTfLiteFloat32:
            info->type = TENSOR_TYPE_FLOAT32;
            break;
        case kTfLiteInt8:
            info->type = TENSOR_TYPE_INT8;
            break;
        case kTfLiteUInt8:
            info->type = TENSOR_TYPE_UINT8;
            break;
        default:
            info->type = TENSOR_TYPE_UNKNOWN;
            break;
    }
    
    info->bytes = tensor->bytes;
    info->dimensions = tensor->dims->size;
    
    for (int i = 0; i < tensor->dims->size && i < 4; i++) {
        info->dims[i] = tensor->dims->data[i];
    }
}

/*============================================================================
 *                      РЕАЛИЗАЦИЯ ПУБЛИЧНЫХ ФУНКЦИЙ
 *============================================================================*/

int ProcessModelOutput_Init(const unsigned char* model_data,
                           size_t model_size,
                           const char** class_names,
                           uint8_t num_classes,
                           const uint16_t* class_colors) {
    
    if (!model_data || model_size == 0) {
        return -1;
    }
    
		
    // Очистка контекста
    memset((void*)&s_ctx, 0, sizeof(InternalContext_t));
    
    // Сохранение пользовательских данных
    if (class_names && num_classes > 0) {
        s_ctx.class_names = class_names;
        s_ctx.num_classes = (num_classes <= MAX_NUM_CLASSES) ? num_classes : MAX_NUM_CLASSES;
    } else {
        s_ctx.class_names = default_class_names;
        s_ctx.num_classes = sizeof(default_class_names) / sizeof(default_class_names[0]);
    }
    
    if (class_colors) {
        s_ctx.class_colors = class_colors;
    } else {
        s_ctx.class_colors = default_class_colors;
    }
    
    // Инициализация статистики
    s_ctx.perf_stats.min_inference_time_ms = 0xFFFFFFFF;
    
    // Загрузка модели
    s_ctx.model = tflite::GetModel(model_data);
    if (!s_ctx.model) {
        return -1;
    }
    
    // Проверка версии
    if (s_ctx.model->version() != TFLITE_SCHEMA_VERSION) {
        Debug_Printf("WARNING: Model version mismatch\r\n");
    }
    
    // Инициализация резолвера
    if (!init_op_resolver()) {
        return -1;
    }
    
    // Создание интерпретатора
    s_ctx.interpreter = new tflite::MicroInterpreter(
        s_ctx.model,
        s_ctx.resolver,
        tensor_arena,
        TENSOR_ARENA_SIZE
    );
    
    if (!s_ctx.interpreter) {
        return -1;
    }
    
    if (s_ctx.interpreter->initialization_status() != kTfLiteOk) {
        return -1;
    }
    
    // Выделение памяти для тензоров
    if (s_ctx.interpreter->AllocateTensors() != kTfLiteOk) {
        return -1;
    }
    
    Debug_Printf("Tensors allocated, used arena: %zu bytes (%.1f%%)\r\n",
                s_ctx.interpreter->arena_used_bytes(),
                (s_ctx.interpreter->arena_used_bytes() * 100.0f) / TENSOR_ARENA_SIZE);
    
    s_ctx.interpreter_initialized = 1;
    
    // Анализ модели
    analyze_model();
    
    return 0;
}

uint8_t* ProcessModelOutput_GetInputBuffer(void) {
    return model_input;
}

void ProcessModelOutput_ConvertCameraFrame(uint16_t* camera_buffer,
                                          uint16_t frame_width,
                                          uint16_t frame_height) {
    if (!camera_buffer) {
        return;
    }
    
    init_scaling_maps(frame_width, frame_height);
    
    for (int out_y = 0; out_y < MODEL_INPUT_HEIGHT; out_y++) {
        int in_y = s_ctx.y_map[out_y] * frame_width;
        
        for (int out_x = 0; out_x < MODEL_INPUT_WIDTH; out_x++) {
            int in_x = s_ctx.x_map[out_x];
            uint16_t rgb565 = camera_buffer[in_y + in_x];
            
            uint8_t r, g, b;
            rgb565_to_rgb888(rgb565, &r, &g, &b);
            
            int out_idx = (out_y * MODEL_INPUT_WIDTH + out_x) * 3;
            model_input[out_idx] = r;
            model_input[out_idx + 1] = g;
            model_input[out_idx + 2] = b;
        }
    }
}

int ProcessModelOutput_RunInference(void) {
    if (!s_ctx.interpreter_initialized) {
        return -1;
    }
    
    // Получение входного тензора
    TfLiteTensor* input = s_ctx.interpreter->input(0);
    if (!input) {
        return -1;
    }
    
    // Копирование данных
    size_t copy_size = (input->bytes < MODEL_INPUT_SIZE) ? 
                       input->bytes : MODEL_INPUT_SIZE;
    
    if (input->type == kTfLiteUInt8) {
        memcpy(input->data.uint8, model_input, copy_size);
    } else if (input->type == kTfLiteInt8) {
        for (size_t i = 0; i < copy_size; i++) {
            input->data.int8[i] = model_input[i] - 128;
        }
    } else {
        memcpy(input->data.raw, model_input, copy_size);
    }
    
    // Запуск инференса
    uint32_t start_time = HAL_GetTick();
    TfLiteStatus status = s_ctx.interpreter->Invoke();
    uint32_t inference_time = HAL_GetTick() - start_time;
    
    if (status != kTfLiteOk) {
        return -1;
    }
    
    // Получение выходного тензора
    TfLiteTensor* output = s_ctx.interpreter->output(0);
    if (!output) {
        return -1;
    }
    
    // Обновление статистики
    update_performance_stats(inference_time);
    s_ctx.last_result.inference_time_ms = inference_time;
    s_ctx.last_result.timestamp_ms = start_time;
    
    // Обработка выходных данных
    int num_outputs = output->bytes;
    uint8_t max_val = 0;
    int max_idx = 0;
    
    if (output->type == kTfLiteUInt8) {
        uint8_t* data = output->data.uint8;
        
        for (int i = 0; i < num_outputs && i < MAX_NUM_CLASSES; i++) {
            s_ctx.last_result.probabilities[i] = data[i];
            
            if (data[i] > max_val) {
                max_val = data[i];
                max_idx = i;
            }
        }
        
    } else if (output->type == kTfLiteInt8) {
        int8_t* data = output->data.int8;
        
        for (int i = 0; i < num_outputs && i < MAX_NUM_CLASSES; i++) {
            uint8_t prob = data[i] + 128;
            s_ctx.last_result.probabilities[i] = prob;
            
            if (prob > max_val) {
                max_val = prob;
                max_idx = i;
            }
        }
    }
    
    s_ctx.last_result.class_id = max_idx;
    s_ctx.last_result.confidence = max_val;
    
    // Логирование результата
    Debug_Printf("\nRESULT: %s (%d), Conf: %d/255 (%.0f%%), Time: %lu ms\r\n",
                s_ctx.class_names[max_idx], max_idx,
                max_val, (max_val * 100.0f) / 255.0f,
                inference_time);
    
    return 0;
}

const InferenceResult_t* ProcessModelOutput_GetLastResult(void) {
    return &s_ctx.last_result;
}

const PerformanceStats_t* ProcessModelOutput_GetPerfStats(void) {
    return &s_ctx.perf_stats;
}

void ProcessModelOutput_ResetStats(void) {
    memset(&s_ctx.perf_stats, 0, sizeof(PerformanceStats_t));
    s_ctx.perf_stats.min_inference_time_ms = 0xFFFFFFFF;
}

ModelState_t ProcessModelOutput_GetState(void) {
    return s_ctx.interpreter_initialized ? MODEL_STATE_READY : MODEL_STATE_UNINITIALIZED;
}

const TensorInfo_t* ProcessModelOutput_GetInputInfo(void) {
    static TensorInfo_t info;
    if (s_ctx.interpreter) {
        fill_tensor_info(s_ctx.interpreter->input(0), &info);
        return &info;
    }
    return NULL;
}

const TensorInfo_t* ProcessModelOutput_GetOutputInfo(void) {
    static TensorInfo_t info;
    if (s_ctx.interpreter) {
        fill_tensor_info(s_ctx.interpreter->output(0), &info);
        return &info;
    }
    return NULL;
}

void ProcessModelOutput_DisplayResult(const InferenceResult_t* result) {
    if (!result || result->class_id < 0 || result->class_id >= s_ctx.num_classes) {
        return;
    }
    
    char disp_str[32];
    uint16_t color = s_ctx.class_colors[result->class_id];
    
		
		
		
		
		
    // Очистка нижней части экрана
    ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, LCD_HEIGHT - RESULT_TEXT_AREA_HEIGHT,
                              LCD_WIDTH, RESULT_TEXT_AREA_HEIGHT, BLACK);
    
    // Рисование рамки
    uint16_t thickness = RESULT_BORDER_THICKNESS;
    ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, 0, LCD_WIDTH, thickness, color);
    ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, LCD_HEIGHT - thickness, 
                              LCD_WIDTH, thickness, color);
    ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, 0, thickness, LCD_HEIGHT, color);
    ST7735_LCD_Driver.FillRect(&st7735_pObj, LCD_WIDTH - thickness, 0, 
                              thickness, LCD_HEIGHT, color);
    
    // Вывод названия класса
    sprintf(disp_str, "%s", s_ctx.class_names[result->class_id]);
    LCD_ShowString(5, LCD_HEIGHT - 35, LCD_WIDTH, 16, 16, (uint8_t*)disp_str);
    
    // Вывод уверенности
    sprintf(disp_str, "%d%%", (result->confidence * 100) / 255);
    LCD_ShowString(5, LCD_HEIGHT - 20, LCD_WIDTH, 16, 12, (uint8_t*)disp_str);
    
    // Вывод времени
    sprintf(disp_str, "%u ms", result->inference_time_ms);
    LCD_ShowString(LCD_WIDTH - 50, LCD_HEIGHT - 20, LCD_WIDTH, 16, 12, (uint8_t*)disp_str);
}

void ProcessModelOutput_AnalyzeModel(void) {
    analyze_model();
}

int ProcessModelOutput_TestWithPattern(void) {
    if (!s_ctx.interpreter_initialized) {
        return -1;
    }
    
    for (int i = 0; i < MODEL_INPUT_SIZE; i++) {
        model_input[i] = (i % 256);
    }
    
    return ProcessModelOutput_RunInference();
}