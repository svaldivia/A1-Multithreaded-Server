#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>

int main(int argc, char **argv) {

    // This is some sample code feel free to delete it

    int i;

    for (i = 0; i < argc; i++) {
        printf("Arg %d is: %s\n", i, argv[i]);
    }

    return 0;

}
