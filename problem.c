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
int agentId, tobaccoSmokerId, paperSmokerId, matchesSmokerId;
int shmid;
int *prices;

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
    shmid = shmget(11111, 3 * sizeof(int), IPC_CREAT|0600);
    prices = (int*)shmat(shmid, NULL, 0); 
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
}

void agent()
{
    srand(time(0));
    for(;;)
    {
        for(int i = 0; i < 3; i++)
        {
            prices[i] = rand() % (SMOKER_STARTING_MONEY / 3) + 1;
            printf("%d ", prices[i]);
        }
        printf("\n");
        sleep(2);
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

