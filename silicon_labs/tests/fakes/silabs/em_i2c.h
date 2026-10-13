/* Fake stand-in for Silicon Labs emlib's em_i2c.h, used ONLY to
 * host-compile and unit-test this vendor's classic I2C adapter under
 * silicon_labs/i2c/hal_port_silabs_i2c.c (TARGET_SILABS). Not a copy
 * of Silicon Labs' header.
 */
#ifndef HAL_WRAPPER_FAKE_EM_I2C_H
#define HAL_WRAPPER_FAKE_EM_I2C_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int _opaque; } I2C_TypeDef;

typedef struct {
    uint32_t freq;
} I2C_Init_TypeDef;

#define I2C_INIT_DEFAULT { .freq = 100000U }

#define I2C_FLAG_WRITE      0x0001U
#define I2C_FLAG_READ       0x0002U
#define I2C_FLAG_WRITE_READ 0x0004U

typedef struct {
    uint8_t *data;
    uint16_t len;
} I2C_SeqBuf_TypeDef;

typedef struct {
    uint16_t addr;
    uint32_t flags;
    I2C_SeqBuf_TypeDef buf[2];
} I2C_TransferSeq_TypeDef;

typedef enum {
    i2cTransferInProgress = 1,
    i2cTransferDone = 0,
    i2cTransferNack = -1,
    i2cTransferBusErr = -2
} I2C_TransferReturn_TypeDef;

extern I2C_TypeDef fake_silabs_i2c0;
extern I2C_TypeDef fake_silabs_i2c1;
#define I2C0 (&fake_silabs_i2c0)
#define I2C1 (&fake_silabs_i2c1)

#define I2C0_IRQn 20
#define I2C1_IRQn 21

void NVIC_EnableIRQ(int irqn);

void I2C_Init(I2C_TypeDef *i2c, I2C_Init_TypeDef const *init);
I2C_TransferReturn_TypeDef I2C_TransferInit(I2C_TypeDef *i2c, I2C_TransferSeq_TypeDef *seq);
I2C_TransferReturn_TypeDef I2C_Transfer(I2C_TypeDef *i2c);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_EM_I2C_H */
