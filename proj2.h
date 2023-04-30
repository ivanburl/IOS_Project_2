#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DOPIS 0
#define BALIK 1
#define PENIZE 2

#define ZAKAZNIK 0
#define USTREDNIK 1

typedef struct {
  int NZ; // number of clients
  int NU; // number of workers
  int TZ; // time for client to go in
  int TU; // time for worker to sleep
  int F;  // office time <F,F/2>
} args_t;

typedef struct {
  int lines;
  int typeCount[3];
  int officeOpened;
} shared;

void processes_init();
void processes_free();

shared *create_mmap(size_t size);
args_t get_args(int argc, char **argv);
int rand_num(int from, int to, int seed);
int rand_arr(int *arr, int from, int to, int seed); // randoms

void clients_gen(int clientsCount, int goTime);    // genereate processes of clients
void workers_gen(int workersCount, int sleepTime); // generate processes of workers

void worker_work(int workerId, int sleepTime); // emulate process of ustrednki
void client_work(int clientId, int goTime);    // emulate process of zakaznik