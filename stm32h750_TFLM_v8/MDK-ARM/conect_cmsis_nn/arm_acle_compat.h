// arm_nn_acle_patch.h
// Патч для недостающих ARM интринсиков CMSIS-NN

#ifndef ARM_NN_ACLE_PATCH_H
#define ARM_NN_ACLE_PATCH_H

// Включаем стандартные интринсики для разных компиляторов
#if defined(__ARMCC_VERSION) || defined(__ARM_COMPILER_VERSION)
    #include <arm_acle.h>
#else
    // Определяем недостающие функции
    #ifndef __SXTB16
        #define __SXTB16(x)                                    \
            (((int32_t)(((x) & 0xFF) << 16) >> 16) |           \
             ((int32_t)(((x) & 0xFF0000) >> 16) << 24) >> 8)
    #endif

    #ifndef __SXTAB16
        #define __SXTAB16(x, y)                                \
            ((int32_t)(((int32_t)(((x) & 0xFF0000) >> 16) +    \
                       ((int32_t)(((y) & 0xFF) << 16) >> 16)) << 16) | \
             ((int32_t)(((x) & 0xFF) + ((y) >> 16)) & 0xFFFF))
    #endif

    #ifndef __ror
        #define __ror(x, n) (((x) >> (n)) | ((x) << (32-(n))))
    #endif

    #ifndef __SXTB16_RORn
        #define __SXTB16_RORn(x, n) __ror(__SXTB16(x), n)
    #endif

    #ifndef __SXTAB16_RORn
        #define __SXTAB16_RORn(x, y, n) __ror(__SXTAB16(x, y), n)
    #endif
#endif

#endif // ARM_NN_ACLE_PATCH_H