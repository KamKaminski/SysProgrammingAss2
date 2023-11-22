#include "types.h"
#include "user.h"

int main(int argc, char *argv[]) {
    // forks process id and check who are we, if above 0 Parent, if 0 Child, or Failure if below 0
    int pid = fork();
    if (pid < 0) {
        printf(1, "init: fork failed\n");
    }
    else if (pid == 0) {
        create_console();
        exec("sh", argv);
        printf(1, "init: exec sh failed\n");
    }
    exit();
}