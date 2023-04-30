#include "proj2.h"

#define SEMAPHORE1_NAME "/xburlu00-writing"
#define SEMAPHORE2_NAME "/xburlu00-DOPIS-asked"
#define SEMAPHORE3_NAME "/xburlu00-BALIK-asked"
#define SEMAPHORE4_NAME "/xburlu00-PENIZE-asked"
#define SEMAPHORE5_NAME "/xburlu00-DOPIS-called"
#define SEMAPHORE6_NAME "/xburlu00-BALIK-called"
#define SEMAPHORE7_NAME "/xburlu00-PENIZE-called"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

sem_t *writing;   // for printing or writing
sem_t *asked[3];  // worker is working on SPECIFIED service and is waiting for client to join
sem_t *called[3]; // client communicating with worker on SPECIFIED service

FILE *out;      // output
args_t *args;   // arguments
shared *status; // shared memory

// processes status
int clientStatus = 0;
int workerStatus = 0;
int mainStatus = 0;

shared *create_mmap(size_t size) { return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0); }

int rand_num(int from, int to, int seed) { // seed is created just for fun (idea is to imporve rand in some way)
  srand(time(0) + seed);
  return from + rand() % (to - from + 1);
}

int rand_arr(int *arr, int from, int to, int seed) {
  int i = rand_num(from, to, seed);
  int j = i;
  while (arr[j] == 0) {
    j++;
    if (j > to) { // cyclic move
      j = from;
    }
    if (j == i) { // all cells from - to are nulls
      return -1;
    }
  }
  return j; // return index
}

args_t get_args(int argc, char *argv[]) {
  if (argc != 6) {
    fprintf(stderr, "Wrong input, you must run the program with 5 arguments!\n");
    exit(EXIT_FAILURE);
  }

  args_t args;
  char *end;
  args.NZ = strtol(argv[1], &end, 10);
  if (*end != '\0' || args.NZ < 0) {
    fprintf(stderr, "Wrong input, number of clients must be >= 0\n");
    exit(EXIT_FAILURE);
  }
  args.NU = strtol(argv[2], &end, 10); // office without workers cant be called office =)
  if (*end != '\0' || args.NU <= 0) {
    fprintf(stderr, "Wrong input, number of workers must be > 0\n");
    exit(EXIT_FAILURE);
  }
  args.TZ = strtol(argv[3], &end, 10);
  if (*end != '\0' || !((args.TZ >= 0) && (args.TZ <= 10000))) {
    fprintf(stderr, "Wrong input, time of client waiting before choosing service must be >= 0 && <= 10000\n");
    exit(EXIT_FAILURE);
  }
  args.TU = strtol(argv[4], &end, 10);
  if (*end != '\0' || !((args.TU >= 0) && (args.TU <= 100))) {
    fprintf(stderr, "Wrong input, time of worker to sleep must be >= 0 && <= 100\n");
    exit(EXIT_FAILURE);
  }
  args.F = strtol(argv[5], &end, 10);
  if (*end != '\0' || !((args.F >= 0) && (args.F <= 10000))) {
    fprintf(stderr, "Wrong input, working time of office must be >= 0 && <= 10000\n");
    exit(EXIT_FAILURE);
  }

  return args;
}

void init_processes() {
  // unlinking sempahores (we are not interested in exit code)
  sem_unlink(SEMAPHORE1_NAME);
  sem_unlink(SEMAPHORE2_NAME);
  sem_unlink(SEMAPHORE3_NAME);
  sem_unlink(SEMAPHORE4_NAME);
  sem_unlink(SEMAPHORE5_NAME);
  sem_unlink(SEMAPHORE6_NAME);
  sem_unlink(SEMAPHORE7_NAME);

  out = fopen("proj2.out", "w");
  // out = stdout;

  if (out == NULL) {
    fprintf(stderr, "Cannot create/open proj2.out file! \n");
    exit(EXIT_FAILURE);
  }

  // Creating all semaphores needed.
  if ((writing = sem_open(SEMAPHORE1_NAME, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) {
    fprintf(stderr, "Cannot create semaphore %s!\n", SEMAPHORE1_NAME);
    exit(EXIT_FAILURE);
  }
  if ((asked[DOPIS] = sem_open(SEMAPHORE2_NAME, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) {
    fprintf(stderr, "Cannot create semaphore %s!\n", SEMAPHORE2_NAME);
    exit(EXIT_FAILURE);
  }
  if ((asked[BALIK] = sem_open(SEMAPHORE3_NAME, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) {
    fprintf(stderr, "Cannot create semaphore %s!\n", SEMAPHORE3_NAME);
    exit(EXIT_FAILURE);
  }
  if ((asked[PENIZE] = sem_open(SEMAPHORE4_NAME, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) {
    fprintf(stderr, "Cannot create semaphore %s!\n", SEMAPHORE4_NAME);
    exit(EXIT_FAILURE);
  }
  if ((called[DOPIS] = sem_open(SEMAPHORE5_NAME, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) {
    fprintf(stderr, "Cannot create semaphore %s!\n", SEMAPHORE5_NAME);
    exit(EXIT_FAILURE);
  }
  if ((called[BALIK] = sem_open(SEMAPHORE6_NAME, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) {
    fprintf(stderr, "Cannot create semaphore %s!\n", SEMAPHORE6_NAME);
    exit(EXIT_FAILURE);
  }
  if ((called[PENIZE] = sem_open(SEMAPHORE7_NAME, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) {
    fprintf(stderr, "Cannot create semaphore %s!\n", SEMAPHORE7_NAME);
    exit(EXIT_FAILURE);
  }

  // Creating shared memory.
  status = create_mmap(sizeof(shared));

  if (status == MAP_FAILED) {
    fprintf(stderr, "Cannot create shared memory!\n");
    exit(EXIT_FAILURE);
  }

  status->lines = 0;
  status->officeOpened = 1;
  status->typeCount[DOPIS] = 0;
  status->typeCount[BALIK] = 0;
  status->typeCount[PENIZE] = 0;
}

void free_processes() {
  int result = munmap(status, sizeof(shared));

  if (result == -1) {
    fprintf(stderr, "Cannot destroy shared memory!\n");
    exit(EXIT_FAILURE);
  }

  if ((sem_close(writing)) == -1) {
    fprintf(stderr, "Cannot close semaphore writing!\n");
    exit(EXIT_FAILURE);
  }
  if ((sem_close(asked[DOPIS])) == -1) {
    fprintf(stderr, "Cannot close semaphore DOPIS asked!\n");
    exit(EXIT_FAILURE);
  }
  if ((sem_close(asked[BALIK])) == -1) {
    fprintf(stderr, "Cannot close semaphore BALIK asked!\n");
    exit(EXIT_FAILURE);
  }
  if ((sem_close(asked[PENIZE])) == -1) {
    fprintf(stderr, "Cannot close semaphore PENIZE asked!\n");
    exit(EXIT_FAILURE);
  }
  if ((sem_close(called[DOPIS])) == -1) {
    fprintf(stderr, "Cannot close semaphore DOPIS called!\n");
    exit(EXIT_FAILURE);
  }
  if ((sem_close(called[BALIK])) == -1) {
    fprintf(stderr, "Cannot close semaphore BALIK called!\n");
    exit(EXIT_FAILURE);
  }
  if ((sem_close(called[PENIZE])) == -1) {
    fprintf(stderr, "Cannot close semaphore PENIZE called!\n");
    exit(EXIT_FAILURE);
  }

  if ((sem_unlink(SEMAPHORE1_NAME)) == -1) {
    fprintf(stderr, "Cannot unlink semaphore %s!\n", SEMAPHORE1_NAME);
    exit(EXIT_FAILURE);
  }
  if ((sem_unlink(SEMAPHORE2_NAME)) == -1) {
    fprintf(stderr, "Cannot unlink semaphore %s!\n", SEMAPHORE2_NAME);
    exit(EXIT_FAILURE);
  }
  if ((sem_unlink(SEMAPHORE3_NAME)) == -1) {
    fprintf(stderr, "Cannot unlink semaphore %s!\n", SEMAPHORE3_NAME);
    exit(EXIT_FAILURE);
  }
  if ((sem_unlink(SEMAPHORE4_NAME)) == -1) {
    fprintf(stderr, "Cannot unlink semaphore %s!\n", SEMAPHORE4_NAME);
    exit(EXIT_FAILURE);
  }
  if ((sem_unlink(SEMAPHORE5_NAME)) == -1) {
    fprintf(stderr, "Cannot unlink semaphore %s!\n", SEMAPHORE5_NAME);
    exit(EXIT_FAILURE);
  }
  if ((sem_unlink(SEMAPHORE6_NAME)) == -1) {
    fprintf(stderr, "Cannot unlink semaphore %s!\n", SEMAPHORE6_NAME);
    exit(EXIT_FAILURE);
  }
  if ((sem_unlink(SEMAPHORE7_NAME)) == -1) {
    fprintf(stderr, "Cannot unlink semaphore %s!\n", SEMAPHORE7_NAME);
    exit(EXIT_FAILURE);
  }

  int test = fclose(out);

  if (test == EOF) {
    fprintf(stderr, "Cannot close output file!\n");
    exit(EXIT_FAILURE);
  }
}

void clients_gen(int clientCount, int goTime) {
  for (int i = 1; i <= clientCount; i++) {
    pid_t client = fork();

    if (client == 0) {

      sem_wait(writing);

      status->lines++;
      fprintf(out, "%d: %c %d: started\n", status->lines, 'Z', i);
      fflush(out);

      sem_post(writing);

      client_work(i, goTime);

      exit(EXIT_SUCCESS);
    }

    if (client < 0) {
      free_processes();
      fprintf(stderr, "Cannot create porcess client!\n");
      exit(EXIT_FAILURE);
    }
  }
}

void workers_gen(int workersCount, int sleepTime) {
  for (int i = 1; i <= workersCount; i++) {
    pid_t worker = fork();

    if (worker == 0) {

      sem_wait(writing);

      status->lines++;
      fprintf(out, "%d: %c %d: started\n", status->lines, 'U', i);
      fflush(out);

      sem_post(writing);

      worker_work(i, sleepTime);

      exit(EXIT_SUCCESS);
    }

    if (worker < 0) {
      free_processes();
      fprintf(stderr, "Cannot create porcess worker!\n");
      exit(EXIT_FAILURE);
    }
  }
}

void worker_work(int workerId, int sleepTime) {
  while (1) {

    sem_wait(writing);

    int workType = rand_arr(status->typeCount, DOPIS, PENIZE, status->lines); // type of work to do for worker

    if (workType == -1) { // nobody is waiting for service

      if (!status->officeOpened) {
        sem_post(writing);
        break;
      }

      status->lines++;
      fprintf(out, "%d: %c %d: taking break\n", status->lines, 'U', workerId);
      fflush(out);

      sem_post(writing);

      usleep(rand_num(0, sleepTime, status->lines) * 1000);

      sem_wait(writing);

      status->lines++;
      fprintf(out, "%d: %c %d: break finished\n", status->lines, 'U', workerId);
      fflush(out);

      sem_post(writing);
    }

    if (workType != -1) { // somebody is waiting for service

      status->typeCount[workType]--;
      status->lines++;
      fprintf(out, "%d: %c %d: serving a service of type %d\n", status->lines, 'U', workerId, workType + 1);
      fflush(out);

      sem_post(writing);

      sem_post(asked[workType]);  // sending signal that client can go in
      sem_wait(called[workType]); // waiting when client will go in

      usleep(rand_num(0, 10, status->lines) * 1000);

      sem_wait(writing);

      status->lines++;
      fprintf(out, "%d: %c %d: service finished\n", status->lines, 'U', workerId);
      fflush(out);

      sem_post(writing);
    }
  }

  sem_wait(writing);

  status->lines++;
  fprintf(out, "%d: %c %d: going home\n", status->lines, 'U', workerId);
  fflush(out);

  sem_post(writing);
}

void client_work(int clientId, int goTime) {
  usleep(rand_num(0, goTime, clientId) * 1000);

  sem_wait(writing);

  if (!status->officeOpened) {

    status->lines++;
    fprintf(out, "%d: %c %d: going home\n", status->lines, 'Z', clientId);
    fflush(out);

    sem_post(writing);
  } else {

    int serviceType = rand_num(DOPIS, PENIZE, clientId); // type of service
    status->lines++;
    fprintf(out, "%d: %c %d: entering office for a service %d\n", status->lines, 'Z', clientId, serviceType + 1);
    fflush(out);

    status->typeCount[serviceType]++;

    sem_post(writing);

    sem_wait(asked[serviceType]); // will wait when worker chooses hime

    sem_wait(writing);

    status->lines++;
    fprintf(out, "%d: %c %d: called by office worker\n", status->lines, 'Z', clientId);
    fflush(out);

    sem_post(called[serviceType]);

    sem_post(writing);

    usleep(rand_num(0, 10, clientId) * 1000);

    sem_wait(writing);

    status->lines++;
    fprintf(out, "%d: %c %d: going home\n", status->lines, 'Z', clientId);
    fflush(out);

    sem_post(writing);
  }
}

int main(int argc, char *argv[]) {
  args_t args = get_args(argc, argv);
  init_processes();

  pid_t mainProcess;
  pid_t clientParent = fork(); // generates people to the office

  if (clientParent < 0) {
    free_processes();
    fprintf(stderr, "Cannot create parent porcess for clients!\n");
    exit(EXIT_FAILURE);
  }

  if (clientParent == 0) {
    clients_gen(args.NZ, args.TZ);
    while ((clientParent = wait(&clientStatus)) > 0)
      ;
    exit(EXIT_SUCCESS);
  }

  if (clientParent > 0) {

    pid_t workerParent = fork();

    if (workerParent < 0) {
      free_processes();
      fprintf(stderr, "Cannot create parent porcess worker!\n");
      exit(EXIT_FAILURE);
    }

    if (workerParent == 0) {
      workers_gen(args.NU, args.TU);
      while ((workerParent = wait(&workerStatus)) > 0)
        ;
      exit(EXIT_SUCCESS);
    }

    if (workerParent > 0) {

      usleep(rand_num(args.F / 2, args.F, 0) * 1000);

      sem_wait(writing);

      status->lines++;
      fprintf(out, "%d: closing\n", status->lines);
      fflush(out);

      status->officeOpened = 0;

      sem_post(writing);

      while (((mainProcess = wait(&clientStatus)) > 0) && ((mainProcess = wait(&workerStatus)) > 0))
        ;
      free_processes();
      exit(EXIT_SUCCESS);
    }
  }

  return 0;
}