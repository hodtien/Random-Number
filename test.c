#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define BUFFER_SIZE 1024
#define DEVICE_NODE "/dev/random_dev"

int main() {
    int ret, fd = -1;
    char buff[BUFFER_SIZE];
    char option = 'q';


    while (1) {
        printf("---- Press 'r' to generate a random number or 'q' to quit the application ----\n");
        scanf(" %c", &option);

        switch (option) {
            case 'r':
                fd = open(DEVICE_NODE, O_RDWR);
                if(fd < 0)
                    printf("=> Can not open the device file\n\n");
                else {
                    ret = read(fd, buff, BUFFER_SIZE);
                    close(fd);

                    printf("=> Random number:\t%s\n\n", buff);
                }
                break;
            case 'q':
                if (fd > -1)
                    close(fd);
                printf("=> Quit the application. Good bye!\n");
                return 0;
            default:
                printf("=> invalid option %c\n\n", option);
                break;
        }
    };
}