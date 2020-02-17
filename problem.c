#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SMOKER_STARTING_MONEY 30
#define AGENT_SLEEP 2
#define SMOKER_SLEEP 1
#define AMOUNT_OF_ITERATIONS 5

#define PRICES_SHM_KEY 10000
#define SMOKER_BALANCE_SHM_KEY 20000
#define PRICES_SEM_KEY 30000
#define TRANSFER_SEM_KEY 40000
#define COMPONENT_MSG_KEY 50000
#define FINISH_SEM_KEY 99999

typedef struct
{
    long id;
    int price;
} Component;

int agentId, tobaccoSmokerId, paperSmokerId, matchesSmokerId;
int pricesShmId, smokersShmId, finishSemId, priceChangeSemId, moneyTransferSemId, componentsMsgId;
int *smokersBalance;
Component *components;

void agent();
void tobaccoSmoker();
void paperSmoker();
void matchesSmoker();
void unlock(int, int, int);
void lock(int, int, int);

int main()
{
    // PAMIEC WSPOLDZIELONA - CENY SKLADNIKOW
    pricesShmId = shmget(PRICES_SHM_KEY, 3 * sizeof(Component), IPC_CREAT|0600);
    if(pricesShmId == -1)
    {
        perror("Utworzenie obszaru pamięci wspoldzielonej (skladniki)");
        exit(1);
    }
    components = (Component*)shmat(pricesShmId, NULL, 0);
    for(int i = 0; i < 3; i++)
    {
        components[i].id = i + 1;
        components[i].price = 5;
    }

    // PAMIEC WSPOLDZIELONA - SALDO PALACZY
    smokersShmId = shmget(SMOKER_BALANCE_SHM_KEY, 3 * sizeof(int), IPC_CREAT|0600);
    if(smokersShmId == -1)
    {
        perror("Utworzenie obszaru pamieci wspoldzielonej (palacze)");
        exit(1);
    }
    smokersBalance = (int*)shmat(smokersShmId, NULL, 0);
    for(int i = 0; i < 3; i++)
        smokersBalance[i] = SMOKER_STARTING_MONEY;

    // SEMAFOR DO ZMIANY CEN SKLADNIKOW
    priceChangeSemId = semget(PRICES_SEM_KEY, 1, IPC_CREAT|0600);
    if(priceChangeSemId == -1)
    {
        perror("Utworzenie semafora (zmiana cen)");
        exit(1);
    }
    if(semctl(priceChangeSemId, 0, SETVAL, 3) == -1)
    {
        perror("Nadanie wartosci semaforowi (zmiana cen)");
        exit(1);
    }

    // SEMAFORY PALACZY DO ZABEZPIECZENIA TRANSFERU PIENIEDZY
    moneyTransferSemId = semget(TRANSFER_SEM_KEY, 3, IPC_CREAT|0600);
    if(moneyTransferSemId == -1)
    {
        perror("Utworzenie semafora (platnosc)");
        exit(1);
    }
    for(int i = 0; i < 3; i++)
        if(semctl(moneyTransferSemId, i, SETVAL, 1) == -1)
        {
            perror("Nadanie wartosci semaforowi (platnosc)");
            exit(1);
        }

    // SEMAFOR DO KONCZENIA PROGRAMU
    finishSemId = semget(FINISH_SEM_KEY, 1, IPC_CREAT|0600);
    if(finishSemId == -1)
    {
        perror("Utworzenie semafora (koniec)");
        exit(1);
    }
    if(semctl(finishSemId, 0, SETVAL, 0) == -1)
    {
        perror("Nadanie wartosci semaforowi (koniec)");
        exit(1);
    }

    // KOLEJKA KOMUNIKATOW (SKALDNIKI)
    componentsMsgId = msgget(COMPONENT_MSG_KEY, IPC_CREAT|0600);
    if(componentsMsgId == -1)
    {
        perror("Utworzenie kolejki komunikatow (zapalki)");
        exit(1);
    }

    // FORKI    
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

    lock(finishSemId, 1, AMOUNT_OF_ITERATIONS); 
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
        lock(priceChangeSemId, 0, 3);
        for(int i = 0; i < 3; i++)
            components[i].price = rand() % (SMOKER_STARTING_MONEY / 3) + 1;
        unlock(priceChangeSemId, 0, 3);
        sleep(AGENT_SLEEP);

        unlock(finishSemId, 1, 1);
    }
}

void tobaccoSmoker()
{
    for(;;)
    {
        lock(priceChangeSemId, 0, 1);
        msgsnd(componentsMsgId, components, sizeof(components[0].price), 0);
        if(smokersBalance[0] >= components[1].price + components[2].price)
        {  
            msgrcv(componentsMsgId, components + 1, sizeof(components[1].price), 1, 0);
            msgrcv(componentsMsgId, components + 2, sizeof(components[2].price), 2, IPC_NOWAIT);
            if (errno == ENOMSG)
                msgsnd(componentsMsgId, components + 1, sizeof(components[1].price), 0);
            else
            {
                lock(moneyTransferSemId, 0, 1);
                smokersBalance[0] -= components[1].price + components[2].price;
                unlock(moneyTransferSemId, 0, 1);
                
                lock(moneyTransferSemId, 1, 1);
                smokersBalance[1] += components[1].price;
                unlock(moneyTransferSemId, 1, 1);

                lock(moneyTransferSemId, 2, 1);
                smokersBalance[2] += components[2].price;
                unlock(moneyTransferSemId, 2, 1);

                sleep(SMOKER_SLEEP); 
            }
        }
        unlock(priceChangeSemId, 0, 1);
    }
}

void paperSmoker()
{

}

void matchesSmoker()
{

}

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
