#ifndef __COMMON_H
#define __COMMON_H
#ifdef __cplusplus
 extern "C" {
#endif

#include "main.h"

void QSPI_Init_Early(void);
void JumpToQSPI(void);

#ifdef __cplusplus
}
#endif

#endif
