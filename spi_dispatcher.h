#ifndef __SPI_DISPATCHER_H__
#define __SPI_DISPATCHER_H__
#include <stdint.h>

#include "sim_avr.h"
#include "sim_irq.h"

/*
generic SPI-dispatcher for simavr

Allows to connect multiple SPI-devices to the same SPI-bus on the AVR.
Each SPI-device must have a separate CS-pin (active low).

(c) 2022 by kittennbfive

AGPLv3+ and NO WARRANTY!

version 11.05.22 01:23
*/

//maximum number of dispatchers in project
#define NB_DISPATCHER_MAX 2

//maximum number of SPI-devices connected to one dispatcher
#define SPI_DEVICES_MAX 5

//maximum length of dispatcher-name
#define SZ_NAME_DISPATCHER 15

//maximum length of device-name connected to dispatcher
#define SZ_NAME_DEVICE 15

//maximum length of IRQ-name for debugging (and to avoid runtime-warnings...)
#define SZ_IRQ_NAME 30

//--do not change anything below this line--

typedef void (*cb_cs_changed_t)(void * device, const uint32_t CS);

typedef uint8_t (*cb_spi_transaction_t)(void * device, const uint8_t rx);

typedef struct
{
	char name[SZ_NAME_DEVICE];
	void * deviceparam;
	uint32_t pin_cs;
	avr_irq_t * irq_cs;
	cb_cs_changed_t cb_cs;
	cb_spi_transaction_t cb_transaction;
} spi_device_t;

typedef struct
{
	char name[SZ_NAME_DISPATCHER];
	char * irq_names[SPI_DEVICES_MAX+2];
	struct avr_t * avr;
	avr_irq_t * irq_base;
	spi_device_t spi_devices[SPI_DEVICES_MAX];
	uint8_t nb_spi_devices;
} spi_dispatcher_t;

//public functions
spi_dispatcher_t * make_new_spi_dispatcher(struct avr_t * avr, char const * const name);
void init_spi_dispatcher(spi_dispatcher_t * const dispatcher, char const * const devicelist, avr_irq_t * spi_in_irq, avr_irq_t * spi_out_irq, ...);

#endif
