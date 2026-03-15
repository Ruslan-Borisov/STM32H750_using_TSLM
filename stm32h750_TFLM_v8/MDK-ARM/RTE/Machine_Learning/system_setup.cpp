/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/lite/micro/system_setup.h"

// Минимально необходимые включения
#include "stm32h7xx_hal.h"
#include "RTE_Components.h"


namespace tflite {

// Пустая реализация - ничего не делаем
void InitializeTarget() {
    // HAL уже должен быть инициализирован в main()
    // Ничего не делаем здесь
}

//// Минимальная реализация DebugLog для вывода ошибок
//extern "C" void DebugLog(const char* s) {
//    // Если есть UART - можно добавить позже
//    // Пока просто заглушка
//    (void)s;
//}

}  // namespace tflite