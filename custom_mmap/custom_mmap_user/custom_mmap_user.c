#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

int main()
{
    int fd = open("/dev/custom_mmap", O_RDWR);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    size_t length = 4096; // Length of memory to map
    void *mapped_mem = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped_mem == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }

    // Use the memory...
	for(int i = 0; i < length; i++)
	{
		*((char*)mapped_mem + i) = 0xBB;
	}

	//Read 10 bytes to check
	unsigned char buffer[10] ={0};
	int bytes_read = read(fd, buffer, 10);
	if(bytes_read < 0)
	{
			printf("Error in read\n");
			goto safe_exit;
			return -1;
	}

	for(int i = 0; i < bytes_read; i++)
	{
		printf("buffer[%d] = %x\n", i , buffer[i]);
	}

    // Unmap and close the device file

safe_exit:
	munmap(mapped_mem, length);
    close(fd);

	printf("Closing User file\n");

    return 0;
}
