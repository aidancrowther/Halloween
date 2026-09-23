/*
 * spi-driver-speed.c
 * 2016-11-23
 * Public Domain
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

/*
spi-driver-speed.c

gcc -o spi-driver-speed spi-driver-speed.c

./spi-driver-speed [bytes [bps [loops] ] ]
*/

#define LOOPS 10000
#define SPEED 1000000
#define BYTES 3

int spiOpen(unsigned spiChan, unsigned spiBaud, unsigned spiFlags)
{
   int i,fd;
   char  spiMode;
   char  spiBits  = 8;
   char dev[32];

   spiMode  = spiFlags & 3;
   spiBits  = 8;

   sprintf(dev, "/dev/spidev0.%d", spiChan);

   if ((fd = open(dev, O_RDWR)) < 0)
   {
      return -1;
   }

   if (ioctl(fd, SPI_IOC_WR_MODE, &spiMode) < 0)
   {
      close(fd);
      return -2;
   }

   if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spiBits) < 0)
   {
      close(fd);
      return -3;
   }

   if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spiBaud) < 0)
   {
      close(fd);
      return -4;
   }

   return fd;
}

int spiClose(int fd)
{
   return close(fd);
}

int spiRead(int fd, unsigned speed, char *buf, unsigned count)
{
   int err;
   struct spi_ioc_transfer spi;

   memset(&spi, 0, sizeof(spi));

   spi.tx_buf        = (unsigned) NULL;
   spi.rx_buf        = (unsigned) buf;
   spi.len           = count;
   spi.speed_hz      = speed;
   spi.delay_usecs   = 0;
   spi.bits_per_word = 8;
   spi.cs_change     = 0;

   err = ioctl(fd, SPI_IOC_MESSAGE(1), &spi);

   return err;
}

int spiWrite(int fd, unsigned speed, char *buf, unsigned count)
{
   int err;
   struct spi_ioc_transfer spi;

   memset(&spi, 0, sizeof(spi));

   spi.tx_buf        = (unsigned) buf;
   spi.rx_buf        = (unsigned) NULL;
   spi.len           = count;
   spi.speed_hz      = speed;
   spi.delay_usecs   = 0;
   spi.bits_per_word = 8;
   spi.cs_change     = 0;

   err = ioctl(fd, SPI_IOC_MESSAGE(1), &spi);

   return err;
}

int spiXfer(int fd, unsigned speed, char *txBuf, char *rxBuf, unsigned count)
{
   int err;
   struct spi_ioc_transfer spi;

   memset(&spi, 0, sizeof(spi));

   spi.tx_buf        = (unsigned long)txBuf;
   spi.rx_buf        = (unsigned long)rxBuf;
   spi.len           = count;
   spi.speed_hz      = speed;
   spi.delay_usecs   = 0;
   spi.bits_per_word = 8;
   spi.cs_change     = 0;

   err = ioctl(fd, SPI_IOC_MESSAGE(1), &spi);

   return err;
}

#define MAX_SPI_BUFSIZ 8192

char RXBuf[MAX_SPI_BUFSIZ];
char TXBuf[MAX_SPI_BUFSIZ];

int bytes = BYTES;
int speed = SPEED;
int loops = LOOPS;

uint32_t randXY(){
   int x = rand()%300;
   int y = rand()%300;

   uint32_t message = ((x << 23 )& 0xFF800000) | ((y << 14) & 0x007FC000) | ((7 << 11) & 0x00003800);

   uint32_t sum = message ^ (message >> 1);
   sum = sum ^ (sum >> 2);
   sum = sum ^ (sum >> 4);
   sum = sum ^ (sum >> 8);
   sum = sum ^ (sum >> 16);

   message |= sum & 1;

   return message;
}

int main(int argc, char * argv[])
{
/*
   int i, fd;
   double start, diff, sps;

   if (argc > 1) bytes = atoi(argv[1]);
   else printf("./spi-driver-speed [bytes [bps [loops] ] ]\n\n");

   if ((bytes < 1) || (bytes > MAX_SPI_BUFSIZ)) bytes = BYTES;

   if (argc > 2) speed = atoi(argv[2]);
   if ((speed < 32000) || (speed > 250000000)) speed = SPEED;

   if (argc > 3) loops = atoi(argv[3]);
   if ((loops < 1) || (loops > 10000000)) loops = LOOPS;

   fd = spiOpen(0, speed, 0);

   start = time_time();

   if (fd < 0) return 1;

   for (i=0; i<loops; i++)
   {
      spiXfer(fd, speed, TXBuf, RXBuf, bytes);
   }

   diff = time_time() - start;

   close(fd);

   printf("sps=%.1f: %d bytes @ %d bps (loops=%d time=%.1f)\n",
      (double)loops / diff, bytes, speed, loops, diff);
*/

   srand(0x69378fba);
   //uint32_t positions = randXY();
   //printf("%X\n", positions);
   //printf("%X\n", 0xFF & positions >> 24);
   //printf("%X\n", 0xFF & positions >> 16);
   //printf("%X\n", 0xFF & positions >> 8);
   //printf("%X\n", 0xFF & positions >> 0);

   int i, fd;
   fd = spiOpen(0, 115200, 0);
   if (fd < 0) return 1;
   char message[4];

   message[0] = 0x10;
message[1] = 0x54;
message[2] = 0x00;
message[3] = 0x00;
spiXfer(fd, 115200, message, NULL, 4);

message[0] = 0x3c;
message[1] = 0x00;
message[2] = 0x38;
message[3] = 0x01;
spiXfer(fd, 115200, message, NULL, 4);

message[0] = 0x60;
message[1] = 0x3c;
message[2] = 0x38;
message[3] = 0x01;
spiXfer(fd, 115200, message, NULL, 4);

message[0] = 0x00;
message[1] = 0x14;
message[2] = 0x38;
message[3] = 0x01;
spiXfer(fd, 115200, message, NULL, 4);

message[0] = 0x78;
message[1] = 0x14;
message[2] = 0x38;
message[3] = 0x01;
spiXfer(fd, 115200, message, NULL, 4);

message[0] = 0x18;
message[1] = 0x3c;
message[2] = 0x38;
message[3] = 0x01;
spiXfer(fd, 115200, message, NULL, 4);

for(int i=0; i<45; i++){
    message[0] = 0x00;
    message[1] = 0x00;
    message[2] = 0x00;
    message[3] = 0x00;
    spiXfer(fd, 115200, message, NULL, 4);
}


}
