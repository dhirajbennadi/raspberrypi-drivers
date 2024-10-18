/*User file for custom_cdev */

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEVICE_PATH "/dev/custom_cdev"
#define IOCTL_PRINT _IOC(_IOC_NONE, 'k',1,0)
#define BUFFER_SIZE 64

int main(int argc, char **argv)
{
        int fd;
        char buffer[BUFFER_SIZE];
        memset(buffer, 'c', sizeof(char) * BUFFER_SIZE);

        fd = open(DEVICE_PATH, O_RDWR);
        if(fd < 0)
        {
                perror("Open Error");
                exit(EXIT_FAILURE);
        }

        /*Write and Read data*/
        char message[] = "Apollo Creed";

        int bytes_written = write(fd, message, strlen(message));

        if(bytes_written < 0)
        {
                printf("Error in write\n");
                close(fd);
                return -1;
        }


        if(lseek(fd, 0, SEEK_SET))
        {
                printf("lseek issue\n");
        }

        int bytes_read = read(fd, buffer, bytes_written);
        if(bytes_read < 0)
        {
                printf("Error in read\n");
                close(fd);
                return -1;
        }

        buffer[bytes_read] = '\0';
        printf("Bytes read from driver = %s\n", &buffer[0]);
        close(fd);

        return 0;
}