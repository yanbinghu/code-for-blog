#include <linux/futex.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


int futex_safe(int* uaddr, int futex_op, int val,
               const struct timespec* timeout, int* uaddr2, int val3) {
  int rc = syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
  if (rc < 0) {
    perror("SYS_futex");
    exit(1);
  }
  return rc;
}


int main(int argc, char** argv) {
  int shm_id = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0666);
  if (shm_id < 0) {
    perror("shmget");
    exit(1);
  }
  int* shared_data = shmat(shm_id, NULL, 0);

  int forkstatus = fork();
  if (forkstatus < 0) {
    perror("fork");
    exit(1);
  }

  if (forkstatus == 0) {
    // Child process

    printf("child waiting for A\n");
    // Wait until 0xA is written to the shared data, taking spurious wake-ups
    // into account.
    futex_safe(shared_data, FUTEX_WAIT, 0xA, NULL, NULL, 0);

    printf("child writing B\n");
    // Write 0xB to the shared data and send a wakeup call.
    *shared_data = 0xB;
    while (!futex_safe(shared_data, FUTEX_WAKE, 1, NULL, NULL, 0)) {
      sched_yield();
    }
  } else {
    // Parent process.

    printf("parent writing A\n");
    // Write 0xA to the shared data and send a wakeup call.
    *shared_data = 0xA;
    while (!futex_safe(shared_data, FUTEX_WAKE, 1, NULL, NULL, 0)) {
      sched_yield();
    }

    printf("parent waiting for B\n");
    // Wait until 0xB is written to the shared data, taking spurious wake-ups
    // into account.
    futex_safe(shared_data, FUTEX_WAIT, 0xB, NULL, NULL, 0);

    wait(NULL);
    exit(0);
  }

  return 0;
}