# simavr-spi-dispatcher

## What is this?
This is a little helper for [simavr](https://github.com/buserror/simavr) that allows to connect multiple devices to the same SPI-bus. Each device must have a separate CS-pin (active low). To make it work on *real hardware* each device must tristate its output (MOSI) when CS is high (don't worry about this for simulation, only worry about it if/once you want to build the real hardware - check datasheet of the corresponding devices).

## Licence and disclaimer
AGPLv3+ and NO WARRANTY!

## How to use / API-overview
I assume you are familiar with simavr already. To make everything work you need the simavr-library and headers, the code that will run on the simulated AVR *and code to simulate each external device (SPI-device in this application) connected to your AVR*. You will also need libelf installed on your system (Debian: `sudo apt install libelf1`).  
simavr-spi-dispatcher only has two public functions:
```
spi_dispatcher_t * make_new_spi_dispatcher(struct avr_t * avr, char const * const name);
void init_spi_dispatcher(spi_dispatcher_t * const dispatcher, char const * const devicelist, avr_irq_t * spi_in_irq, avr_irq_t * spi_out_irq, ...);
```

### First step: Create a new dispatcher
Call `make_new_spi_dispatcher()` *after having initialized the AVR (`avr_init()`)*. The arguments of this function should be pretty self-explanatory...

### Second step: Connect the dispatcher to the AVR and the SPI-devices
Call `init_spi_dispatcher()` with the following arguments:
* A pointer to the dispatcher-structure as returned by `make_new_spi_dispatcher()`.
* A list of device-names separated by ',' - for example "nRF,RAM,SD" if you want to connect a nRF24L01+, some external SPI-RAM and a SD-card to the SPI-bus on your AVR.
* The IRQ for SPI-input of the AVR (MOSI) as returned by `avr_io_getirq()`.
* The IRQ for SPI-output of the AVR (MISO) as returned by `avr_io_getirq()`.
* For each device you need need to specifiy *four additional arguments*, see below.

#### Additional arguments for each SPI-device for `init_spi_dispatcher()`
1) A pointer ( `void *` ) to some data structure used by your SPI-device. This argument is simply passed to the callback-functions for each simulated device so you can have multiple instances of the same SPI-device connected to your AVR (or even several AVR inside the simulation, see [this issue](https://github.com/buserror/simavr/issues/476)) if supported by the simulation code of the device. This parameter can be `NULL` if not needed.
2) The IRQ for the CS-pin of the device as returned by `avr_io_getirq()`. Mandatory.
3) A pointer to a callback-function (of type `cb_cs_changed_t`) from the simulator code of the device that will be called when the CS-pin of the device changes state. You can specify `NULL` if you don't need this callback.
4) A pointer to a callback-function (of type `cb_spi_transaction_t`) from the simulator code of the device that will be called for each SPI-read-write to the device (onyl if CS is low). Mandatory.

## API to be provided by simulation code for a device to be used/connected through this dispatcher
### callback-function for CS-pin level change
```
typedef void (*cb_cs_changed_t)(void * device, const uint32_t CS);
```
The first argument is the pointer to some data structure that was specified as the first additional argument to `init_spi_dispatcher()`. It is not touched by the dispatcher and can be `NULL`.
The second argument is the new level of the CS-pin (0 or 1). Use this for example to reset the state of the SPI-interface of your device when CS goes high. I used a `uint32_t` instead of a simple `bool` for CS because that's what simavr uses internally.

### callback-function for SPI-read-write-transaction
```
typedef uint8_t (*cb_spi_transaction_t)(void * device, const uint8_t rx);
```
The first argument is the same as described above.
The second argument is the byte sent by the AVR (==MOSI) to be used by your simulation code.
The function returns what your device sends back to the AVR (==MISO).

## Example
In this example i want to connect three devices to the SPI**1** (ie the **USART1 used as a SPI-master**, thats why in the code i use `AVR_IOCTL_*UART*_GETIRQ`) of an Atmega1284P:
* A nRF24L01+
* some external RAM
* a SD-card

The nRF has its CE-pin (called CSN, don't confuse this with the CE-pin for RX/TX!) connected to PD6. There are two nRF (and two AVR, second not shown here) inside the "circuit" so the simulation code of the nRF needs a pointer to the correct data structure.  
The RAM has its CE-pin connected to PC2. The pointer to the device-internal data structure is `NULL`, the simulation code of the RAM does not support multiple instances and will ignore this argument.  
The SD-card has its CE-pin connected to PD7. Same thing for the pointer as above.
```
spi_dispatcher_t * dispatch1=make_new_spi_dispatcher(avr1, "AVR1");
init_spi_dispatcher(dispatch1, "nRF,RAM,SD", avr_io_getirq(avr1, AVR_IOCTL_UART_GETIRQ('1'), UART_IRQ_OUTPUT), avr_io_getirq(avr1, AVR_IOCTL_UART_GETIRQ('1'), UART_IRQ_INPUT), \
					nRF1, avr_io_getirq(avr1, AVR_IOCTL_IOPORT_GETIRQ('D'), 6), &csn_nRF, &spi_nRF, \
					NULL, avr_io_getirq(avr1, AVR_IOCTL_IOPORT_GETIRQ('C'), 2), &ce_RAM, &spi_RAM, \
					NULL, avr_io_getirq(avr1, AVR_IOCTL_IOPORT_GETIRQ('D'), 7), &ce_SD, &spi_SD);
```

## Limitations / Other stuff
* By default the code is configured for a maximum of two dispatchers with maximum five SPI-devices each inside your project. You can adjust these parameters inside the header-file.
* As said above the CS-pin of your device must be *active low*.
* If at some point there is more than one selected SPI-device (more than one CS-pin low) the dispatcher will abort simulation with an error (bus collision).
