#include "common.h"
#include "w25qxx_qspi.h"
#include "quadspi.h"
#include "main.h"


typedef void (*pFunction)(void);


void QSPI_Init_Early(void)
{
    MX_QUADSPI_Init();

    // 1. Reset
    QSPI_ResetDevice(&hqspi);
    HAL_Delay(1);

    // 2. Включить Quad mode (бит QE!)
    uint8_t sr2 = w25qxx_ReadSR(W25X_ReadStatusReg2);
    if(!(sr2 & 0x02))
    {
        W25qxx_WriteEnable();
        w25qxx_WriteSR(W25X_WriteStatusReg2, sr2 | 0x02);
        W25QXX_Wait_Busy();
    }

    // 3. Включаем memory mapped mode
    w25qxx_Startup(0);
}





void JumpToQSPI(void)
{
    uint32_t sp = *(uint32_t*)0x90000000;
    uint32_t reset = *(uint32_t*)0x90000004;

    if((sp & 0x2FFE0000) != 0x20000000) return;

    __disable_irq();

    SCB->VTOR = 0x90000000;   // < ТОЛЬКО ЗДЕСЬ

    __set_MSP(sp);

    ((void (*)(void))reset)();
}