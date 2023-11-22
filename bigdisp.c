#include "types.h"
#include "user.h"
// prints out numbers in sequence, used to test keyboard and screen
int main(int argc, char *argv[]) {
    int value = 0;

    while (value < 3000)
    {
    printf(1, "%d \n", value);
    value++;
    sleep(1);
    }
    exit();
}