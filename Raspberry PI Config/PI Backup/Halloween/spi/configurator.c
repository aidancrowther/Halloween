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

#define ACCELERATION_MASK 0xFFFFF000
#define ACCELERATION_SHIFT 12
#define TRANSFER_SIZE_MASK 0x00000FFE
#define TRANSFER_SIZE_SHIFT 1
#define MAX_SPEED_MASK 0xFFFFC000
#define MAX_SPEED_SHIFT 14
#define MIN_SPEED_MASK 0x00003FFE
#define MIN_SPEED_SHIFT 1
#define X_HOME_MASK 0xFFF00000
#define X_HOME_SHIFT 20
#define Y_HOME_MASK 0x000FFF00
#define Y_HOME_SHIFT 8
#define PROJECTOR_ID_MASK 0x000000F0
#define PROJECTOR_ID_SHIFT 4

void generate_header(char* header, int target){

   int message = (target << 28) | (1 << 17);

   int sum = message ^ (message >> 1);
   sum = sum ^ (sum >> 2);
   sum = sum ^ (sum >> 4);
   sum = sum ^ (sum >> 8);
   sum = sum ^ (sum >> 16);
   
   message = message ^ (sum & 1);

   header[0] = (message & 0xFF000000) >> 24;
   header[1] = (message & 0x00FF0000) >> 16;
   header[2] = (message & 0x0000FF00) >> 8;
   header[3] = (message & 0x000000FF) >> 0;

}

int checksum(int message){
   int sum = message ^ (message >> 1);
   sum = sum ^ (sum >> 2);
   sum = sum ^ (sum >> 4);
   sum = sum ^ (sum >> 8);
   sum = sum ^ (sum >> 16);
   
   message = message ^ (sum & 1);

   return message;
}

int main(int argc, char * argv[])
{

   int acceleration = ACCELERATION_MASK >> (ACCELERATION_SHIFT);
   int transfer_size = TRANSFER_SIZE_MASK >> (TRANSFER_SIZE_SHIFT);
   int max_speed = MAX_SPEED_MASK >> MAX_SPEED_SHIFT;
   int min_speed = MIN_SPEED_MASK >> MIN_SPEED_SHIFT;
   int x_home = X_HOME_MASK >> X_HOME_SHIFT;
   int y_home = Y_HOME_MASK >> Y_HOME_SHIFT;
   int projector_id = PROJECTOR_ID_MASK >> PROJECTOR_ID_SHIFT;
   int target_id = 0;

   for(int i=1; i<argc; i++){

      if(!strcmp(argv[i], "acceleration")){
         acceleration = atoi(argv[i+1]);
      }

      if(!strcmp(argv[i], "transfer_size")){
         transfer_size = atoi(argv[i+1]);
      }

      if(!strcmp(argv[i], "max_speed")){
         max_speed = atoi(argv[i+1]);
      }

      if(!strcmp(argv[i], "min_speed")){
         min_speed = atoi(argv[i+1]);
      }

      if(!strcmp(argv[i], "x_home")){
         x_home = atoi(argv[i+1]);
      }

      if(!strcmp(argv[i], "y_home")){
         y_home = atoi(argv[i+1]);
      }

      if(!strcmp(argv[i], "projector_id")){
         projector_id = atoi(argv[i+1]);
      }

      if(!strcmp(argv[i], "target_id")){
         target_id = atoi(argv[i+1]);
      }

   }

   int i, fd;
   fd = spiOpen(0, 115200, 0);
   if (fd < 0) return 1;

   char header[4];
   generate_header(header, target_id);

   printf("%X %X %X %X\n", header[0], header[1], header[2], header[3]);
   spiXfer(fd, 115200, header, NULL, 4);

   int message = 0;
   char config[4];

   message = ((acceleration << ACCELERATION_SHIFT) & ACCELERATION_MASK) | ((transfer_size << TRANSFER_SIZE_SHIFT) & TRANSFER_SIZE_MASK);
   message = checksum(message);
   config[0] = (message & 0xFF000000) >> 24;
   config[1] = (message & 0x00FF0000) >> 16;
   config[2] = (message & 0x0000FF00) >> 8;
   config[3] = (message & 0x000000FF) >> 0;

   printf("%X %X %X %X\n", config[0], config[1], config[2], config[3]);
   spiXfer(fd, 115200, config, NULL, 4);

   message = ((max_speed << MAX_SPEED_SHIFT) & MAX_SPEED_MASK) | ((min_speed << MIN_SPEED_SHIFT) & MIN_SPEED_MASK);
   message = checksum(message);
   config[0] = (message & 0xFF000000) >> 24;
   config[1] = (message & 0x00FF0000) >> 16;
   config[2] = (message & 0x0000FF00) >> 8;
   config[3] = (message & 0x000000FF) >> 0;

   printf("%X %X %X %X\n", config[0], config[1], config[2], config[3]);
   spiXfer(fd, 115200, config, NULL, 4);

   message = ((x_home << X_HOME_SHIFT) & X_HOME_MASK) | ((y_home << Y_HOME_SHIFT) & Y_HOME_MASK) | ((projector_id << PROJECTOR_ID_SHIFT) & PROJECTOR_ID_MASK);
   message = checksum(message);
   config[0] = (message & 0xFF000000) >> 24;
   config[1] = (message & 0x00FF0000) >> 16;
   config[2] = (message & 0x0000FF00) >> 8;
   config[3] = (message & 0x000000FF) >> 0;

   printf("%X %X %X %X\n", config[0], config[1], config[2], config[3]);
   spiXfer(fd, 115200, config, NULL, 4);

   for(int i=0; i<47; i++){
      config[0] = 0x00;
      config[1] = 0x00;
      config[2] = 0x00;
      config[3] = 0x00;
      spiXfer(fd, 115200, config, NULL, 4);
   }

    /*
   int i, fd;
   fd = spiOpen(0, 115200, 0);
   if (fd < 0) return 1;
   char message[4];


   message[0] = 0x10;
   message[1] = 0x14;
   message[2] = 0x00;
   message[3] = 0x01;

   spiXfer(fd, 115200, message, NULL, 4);

   for(int i=0; i<50; i++){
      message[0] = 0x00;
      message[1] = 0x00;
      message[2] = 0x3F;
      message[3] = 0xE1;

      spiXfer(fd, 115200, message, NULL, 4);
   }
   */

   return 0;
   
}
