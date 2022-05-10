#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <err.h>

#include "spi_dispatcher.h"

#include "sim_avr.h"
#include "avr_spi.h"

/*
generic SPI-dispatcher for simavr

Allows to connect multiple SPI-devices to the same SPI-bus on the AVR.
Each SPI-device must have a separate CS-pin (active low).

(c) 2022 by kittennbfive

AGPLv3+ and NO WARRANTY!

version 11.05.22 01:21
*/

static spi_dispatcher_t dispatcher[NB_DISPATCHER_MAX];
static uint8_t nb_dispatcher=0;

spi_dispatcher_t * make_new_spi_dispatcher(struct avr_t * avr, char const * const name)
{
	if(nb_dispatcher==NB_DISPATCHER_MAX)
		errx(1, "make_new_spi_dispatcher: maximum number of SPI-dispatchers reached, increase NB_DISPATCHER_MAX");

	spi_dispatcher_t * ret=&dispatcher[nb_dispatcher];

	strncpy(dispatcher[nb_dispatcher].name, name, SZ_NAME_DISPATCHER-1);
	dispatcher[nb_dispatcher].avr=avr;
	dispatcher[nb_dispatcher].nb_spi_devices=0;

	nb_dispatcher++;

	return ret;
}

enum
{
	DISPATCH_SPI_IN=0,
	DISPATCH_SPI_OUT,
	DISPATCH_CE_IN
};

static void cb_spi_rx_tx(struct avr_irq_t * irq, uint32_t value, void * param)
{
	(void)irq;
	spi_dispatcher_t * disp=(spi_dispatcher_t *)param;

	uint8_t to_send=0xff;

	uint8_t i;
	uint8_t count_cs_low=0;
	uint8_t i_active_device=0;
	for(i=0; i<disp->nb_spi_devices; i++)
	{
		if(disp->spi_devices[i].pin_cs==0)
		{
			i_active_device=i;
			count_cs_low++;
		}
	}

	if(count_cs_low>1)
		errx(1, "SPI-dispatcher %s: SPI bus collision: multiple CS are low", disp->name);

	if(count_cs_low==1)
	{
		if(!disp->spi_devices[i_active_device].cb_transaction)
			errx(1, "SPI-dispatcher %s: no SPI-transaction callback found for device %s", disp->name, disp->spi_devices[i_active_device].name);

		to_send=disp->spi_devices[i_active_device].cb_transaction(disp->spi_devices[i_active_device].deviceparam, value&0xff);
	}

	avr_raise_irq(disp->irq_base+DISPATCH_SPI_OUT, to_send);
}

static void cb_spi_cs_pin(struct avr_irq_t * irq, uint32_t value, void * param)
{
	(void)irq;

	spi_device_t * dev=(spi_device_t*)param;

	if(dev->cb_cs)
		dev->cb_cs(dev->deviceparam, value);

	dev->pin_cs=value;
}

void init_spi_dispatcher(spi_dispatcher_t * const dispatcher, char const * const devicelist, avr_irq_t * spi_in_irq, avr_irq_t * spi_out_irq, ...)
{
	char buf[100];
	strncpy(buf, devicelist, 100);

	va_list ap;
	va_start(ap, spi_out_irq);

	char * device;
	device=strtok(buf, ",");

	if(device==NULL)
		errx(1, "init_spi_dispatcher: invalid config string");

	void * deviceparam;
	avr_irq_t * irq_cs;
	cb_cs_changed_t cb_cs;
	cb_spi_transaction_t cb_spi;

	do
	{
		if(dispatcher->nb_spi_devices==SPI_DEVICES_MAX)
			errx(1, "init_spi_dispatcher: %s: maximum number of SPI-devices reached, increase SPI_DEVICE_MAX", dispatcher->name);

		strncpy(dispatcher->spi_devices[dispatcher->nb_spi_devices].name, device, SZ_NAME_DEVICE-1);

		deviceparam=va_arg(ap, void *);
		dispatcher->spi_devices[dispatcher->nb_spi_devices].deviceparam=deviceparam;

		irq_cs=va_arg(ap, avr_irq_t *);
		dispatcher->spi_devices[dispatcher->nb_spi_devices].irq_cs=irq_cs;

		cb_cs=va_arg(ap, cb_cs_changed_t);
		dispatcher->spi_devices[dispatcher->nb_spi_devices].cb_cs=cb_cs;

		cb_spi=va_arg(ap, cb_spi_transaction_t);
		dispatcher->spi_devices[dispatcher->nb_spi_devices].cb_transaction=cb_spi;

		dispatcher->nb_spi_devices++;
	}
	while((device=strtok(NULL, ",")));

	va_end(ap);

	dispatcher->irq_names[0]=malloc(SZ_IRQ_NAME);
	dispatcher->irq_names[1]=malloc(SZ_IRQ_NAME);

	sprintf(dispatcher->irq_names[0], "SPI_dispatch_%s_IN", dispatcher->name);
	sprintf(dispatcher->irq_names[1], "SPI_dispatch_%s_OUT", dispatcher->name);

	uint8_t i;
	for(i=0; i<dispatcher->nb_spi_devices; i++)
	{
		dispatcher->irq_names[DISPATCH_CE_IN+i]=malloc(SZ_IRQ_NAME);
		sprintf(dispatcher->irq_names[DISPATCH_CE_IN+i], "SPI_dispatch_%s_CE_%s", dispatcher->name, dispatcher->spi_devices[i].name);
	}
	for(; i<SPI_DEVICES_MAX; i++)
		dispatcher->irq_names[DISPATCH_CE_IN+i]=NULL;

	dispatcher->irq_base=avr_alloc_irq(&(dispatcher->avr->irq_pool), 0, DISPATCH_CE_IN+dispatcher->nb_spi_devices, (const char**)dispatcher->irq_names);

	avr_connect_irq(spi_in_irq, dispatcher->irq_base+DISPATCH_SPI_IN);
	avr_connect_irq(dispatcher->irq_base+DISPATCH_SPI_OUT, spi_out_irq);
	avr_irq_register_notify(dispatcher->irq_base+DISPATCH_SPI_IN, &cb_spi_rx_tx, dispatcher);

	for(i=0; i<dispatcher->nb_spi_devices; i++)
	{
		if(!dispatcher->spi_devices[i].irq_cs)
			errx(1, "init_spi_dispatcher: %s: no CS-pin for SPI-device %s found\n", dispatcher->name, dispatcher->spi_devices[i].name);
		avr_connect_irq(dispatcher->spi_devices[i].irq_cs, dispatcher->irq_base+DISPATCH_CE_IN+i);
		avr_irq_register_notify(dispatcher->irq_base+DISPATCH_CE_IN+i, &cb_spi_cs_pin, &dispatcher->spi_devices[i]);
	}

	printf("SPI-dispatcher %s: %u devices successfully connected\n", dispatcher->name, dispatcher->nb_spi_devices);
}

