#include "tensorflow/lite/micro/micro_time.h"
#include "RTE_Components.h"
#include "stm32h7xx_hal.h"

namespace tflite {

#if defined(PROJECT_GENERATION)

// Stub functions for the project_generation target
uint32_t ticks_per_second() { return 0; }
uint32_t GetCurrentTimeTicks() { return 0; }

#else

// Get the CPU frequency for accurate time conversion
uint32_t ticks_per_second() { 
    return HAL_RCC_GetSysClockFreq(); 
}

uint32_t GetCurrentTimeTicks() {
    static bool is_initialized = false;

    if (!is_initialized) {
#if (!defined(TF_LITE_STRIP_ERROR_STRINGS) && !defined(ARMCM0) && \
     !defined(ARMCM0plus))
        
        // For STM32H7 (Cortex-M7)
        #ifdef ARMCM7
            // Unlock DWT (required on some Cortex-M7 implementations)
            DWT->LAR = 0xC5ACCE55;
        #endif
        
        // Enable trace and DWT
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        
        // Reset cycle counter
        DWT->CYCCNT = 0;
        
        // Enable cycle counter
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        
        // Optional: Also enable PMU counters for more options
        #ifdef ARM_MODEL_USE_PMU_COUNTERS
            ARM_PMU_Enable();
            ARM_PMU_CYCCNT_Reset();
            ARM_PMU_CNTR_Enable(PMU_CNTENSET_CCNTR_ENABLE_Msk);
        #endif
#endif
        is_initialized = true;
    }

#if (!defined(TF_LITE_STRIP_ERROR_STRINGS) && !defined(ARMCM0) && \
     !defined(ARMCM0plus))
    
    #ifdef ARM_MODEL_USE_PMU_COUNTERS
        return ARM_PMU_Get_CCNTR();
    #else
        return DWT->CYCCNT;
    #endif
    
#else
    return 0;
#endif
}

#endif  // defined(PROJECT_GENERATION)

}  // namespace tflite