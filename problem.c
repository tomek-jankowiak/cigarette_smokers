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
#define PRICES_SHM_KEY 10000
#define PRICES_SEM_KEY 20000
#define TRANSFER_SEM_KEY 30000
#define TOBACCO_MSG_KEY 40000
#define PAPER_MSG_KEY 50000
#define MATCHES_MSG_KEY 60000

int agentId, tobaccoSmokerId, paperSmokerId, matchesSmokerId;
int shmid, priceChangeSemId, moneyTransferSemId, tobaccoMsgId, paperMsgId, matchesPaperId;
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

struct component
{
    int id;
    int price;
};

int main()
{
    // PAMIEC WSPOLDZIELONA - CENY SKLADNIKOW
    shmid = shmget(PRICES_SHM_KEY, 3 * sizeof(int), IPC_CREAT|0600);
    if(shmid == -1)
    {
        perror("Utworzenie obszaru pamięci wspoldzielonej");
        exit(1);
    }
    prices = (int*)shmat(shmid, NULL, 0);

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
        lock(semid, 0, 3);
        for(int i = 0; i < 3; i++)
            prices[i] = rand() % (SMOKER_STARTING_MONEY / 3) + 1;
        unlock(semid, 0, 3);
        
        iterations++;
        sleep(AGENT_SLEEP);
        if(iterations >= 5)
            break;
    }
}

void tobaccoSmoker()
{
    struct component tobacco;
    tobacco.id = 0;

}

void paperSmoker()
{
    struct component paper;
    paper.id = 1;
}

void matchesSmoker()
{
    struct component matches;
    matches.id = 2;
}

