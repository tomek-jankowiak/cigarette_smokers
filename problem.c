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
#define AGENT_SLEEP 2
#define SMOKER_SLEEP 1
#define COMPONENT_ON_TABLE 1
#define COMPONENT_NOT_ON_TABLE 0
#define PRICES_SHM_KEY 10000
#define SMOKER_BALANCE_SHM_KEY 20000
#define PRICES_SEM_KEY 30000
#define TRANSFER_SEM_KEY 40000
#define TOBACCO_MSG_KEY 50000
#define PAPER_MSG_KEY 60000
#define MATCHES_MSG_KEY 70000

typedef struct
{
    short on_table;
    int price;
} Component;

int agentId, tobaccoSmokerId, paperSmokerId, matchesSmokerId;
int pricesShmId, smokersShmId, priceChangeSemId, moneyTransferSemId, tobaccoMsgId, paperMsgId, matchesMsgId;
int *smokersBalance, iterations = 0;
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
        components[i].on_table = 0;
        components[i].price = 5;
    }

    // PAMIEC WSPOLDZIELONA - SALDO PALACZY
    smokersShmId = shmget(SMOKER_BALANCE_SHM_KEY, 3 * sizeof(int), IPC_CREAT|0600);
    if(smokersShmId == -1)
    {
        perror("Utworzenie obszaru pamieci wspoldzielonej (palacze)");
        exit(1);
    }
    smokersBalance = (*int)shmat(smokersShmId, NULL, 0);
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

    // KOLEJKA KOMUNIKATOW (TYTOŃ) 
    tobaccoMsgId = msgget(TOBACCO_MSG_KEY, IPC_CREAT|0600);
    if(tobaccoMsgId == -1)
    {
        perror("Utworzenie kolejki komunikatow (tyton)");
        exit(1);
    }
    // KOLEJKA KOMUNIKATOW (PAPIER) 
    paperMsgId = msgget(PAPER_MSG_KEY, IPC_CREAT|0600);
    if(paperMsgId == -1)
    {
        perror("Utworzenie kolejki komunikatow (papier)");
        exit(1);
    }
    // KOLEJKA KOMUNIKATOW (ZAPALKI) 
    matchesMsgId = msgget(MATCHES_MSG_KEY, IPC_CREAT|0600);
    if(matchesMsgId == -1)
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
        lock(priceChangeSemId, 0, 3);
        for(int i = 0; i < 3; i++)
            components[i].price = rand() % (SMOKER_STARTING_MONEY / 3) + 1;
        unlock(priceChangeSemId, 0, 3);
        
        iterations++;
        sleep(AGENT_SLEEP);
        if(iterations >= 5)
            break;
    }
}

void tobaccoSmoker()
{
    for(;;)
    {
        components[0].on_table = COMPONENT_ON_TABLE;
        msgsnd(tobaccoMsgId, &components[0], sizeof(components[0].on_table), 0);
        
        lock(priceChangeSemId, 0, 1);
        if(smokersBalance[0] >= components[1].price + components[2].price)
        {
            msgrcv(paperMsgId, &components[1], sizeof(components[1].on_table), COMPONENT_ON_TABLE);
            msgrcv(matchesMsgId, &components[2], sizeof(components[2].on_table), COMPONENT_ON_TABLE);
            components[1].on_table = COMPONENT_NOT_ON_TABLE;
            components[2].on_table = COMPONENT_NOT_ON_TABLE;
            msgsnd(paperMsgId, &components[1], sizeof(components[1].on_table), 0);
            msgsnd(matchesMsgId, &components[2], sizeof(components[2].on_table), 0);
            
            lock(moneyTransferSemId, 0, 1);
            smokersBalance[0] -= components[1].price + components[2].price;
            unlock(moneyTransferSemId, 0, 1);
            
            lock(moneyTransferSemId, 1, 1);
            smokersBalance[1] += components[1].price;
            unlock(moneyTransferSemId, 1, 1);

            lock(moneyTransferSemId, 2, 1);
            smokersBalance[2] += components[2].price;
            unlock(moneyTransferSemId, 2, 1);
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
