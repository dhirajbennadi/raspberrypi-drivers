#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main ()
{
	int returnValue;
	int fd = open("/dev/custom_gpio", O_RDWR);

	if(fd < 0)
	{
		perror("Failed to open device");
		return -1;
	}

	while (1)
	{
		lseek(fd, 0, SEEK_SET);
		returnValue = write(fd, "21,1", 4);
		usleep(500000);
		lseek(fd, 0, SEEK_SET);
		write(fd, "21,0", 4);
		usleep(500000);
	}
}