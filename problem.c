#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SMOKER_STARTING_MONEY 30
#define PRICES_SHM_KEY 10000
#define PRICES_SEM_KEY 20000
int agentId, tobaccoSmokerId, paperSmokerId, matchesSmokerId;
int shmid, semid;
int *prices, iterations = 0;

void agent();
void tobaccoSmoker();
void paperSmoker();
void matchesSmoker();

void unlock(int semid, int semnum, int val)
{
    struct sembuf buffer;

    buffer.sem_num = semnum;
    buffer.sem_op = val;
    buffer.sem_flg = 0;
    if(semop(semid, &buffer, 1) == -1)
    {
        perror("Podniesienie semafora");
        exit(1);
    }
}

void lock(int semid, int semnum, int val)
{
    struct sembuf buffer;

    buffer.sem_num = semnum;
    buffer.sem_op = (-1) * val;
    buffer.sem_flg = 0;
    if(semop(semid, &buffer, 1) == -1)
    {
        perror("Opuszczenie semafora");
        exit(1);
    }
}

int main()
{
    shmid = shmget(PRICES_SHM_KEY, 3 * sizeof(int), IPC_CREAT|0600);
    if(shmid == -1)
    {
        perror("Utworzenie obszaru pamięci wspoldzielonej");
        exit(1);
    }
    prices = (int*)shmat(shmid, NULL, 0); 

    semid = semget(PRICES_SEM_KEY, 1, IPC_CREAT|0600);
    if(semid == -1)
    {
        perror("Utworzenie tablicy semaforow");
        exit(1);
    }

    if(semctl(semid, 0, SETVAL, 3) == -1)
    {
        perror("Nadanie wartosci semaforowi");
        exit(1);
    }

    switch(agentId = fork())
    {
        case -1:
            perror("Utworzenie procesu agenta");
            exit(1);
        case 0:
            agent();
    }
    switch(tobaccoSmokerId = fork())
    {
        case -1:
            perror("Utworzenie procesu palacza 1");
            exit(1);
        case 0:
            tobaccoSmoker();
    }
    switch(paperSmokerId = fork())
    {
        case -1:
            perror("Utworzenie procesu palacza 2");
            exit(1);
        case 0:
            paperSmoker();
    }
    switch(matchesSmokerId = fork())
    {
        case -1:
            perror("Utworzenie procesu palacza 3");
            exit(1);
        case 0:
            matchesSmoker();
    }
    while(iterations <= 5);
    kill(agentId, 9);
    kill(tobaccoSmokerId, 9);
    kill(paperSmokerId, 9);
    kill(matchesSmokerId, 9);
}

void agent()
{
    srand(time(0));
    for(;;)
    {
        lock(semid, 0, 3);
        for(int i = 0; i < 3; i++)
        {
            prices[i] = rand() % (SMOKER_STARTING_MONEY / 3) + 1;
            printf("%d ", prices[i]);
        }
        unlock(semid, 0, 3);
        iterations++;
        printf("\n");
        sleep(2);
        if(iterations >= 5)
            break;
    }
}

void tobaccoSmoker()
{

}

void paperSmoker()
{

}

void matchesSmoker()
{

}

