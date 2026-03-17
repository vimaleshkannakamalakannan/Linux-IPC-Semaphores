 #include <stdio.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <sys/types.h>
 #include <sys/ipc.h>
 #include <sys/sem.h>
 #include <sys/shm.h>
 #include <sys/wait.h>

#define BUFFER_SIZE 3

union semun {
    int val;
};

struct shared {
    int buffer[BUFFER_SIZE];
    int in, out;
};

void sem_wait(int semid, int num) {
    struct sembuf sb = {num, -1, 0};
    semop(semid, &sb, 1);
}

void sem_signal(int semid, int num) {
    struct sembuf sb = {num, 1, 0};
    semop(semid, &sb, 1);
}

int main() {
    int shmid = shmget(IPC_PRIVATE, sizeof(struct shared), 0666|IPC_CREAT);
    struct shared *s = (struct shared*)shmat(shmid, NULL, 0);
    s->in = 0; 
    s->out = 0;
    
    int semid = semget(IPC_PRIVATE, 3, 0666|IPC_CREAT);
    union semun su;
    su.val = 1; semctl(semid, 0, SETVAL, su); // mutex
    su.val = BUFFER_SIZE; semctl(semid, 1, SETVAL, su); // empty
    su.val = 0; semctl(semid, 2, SETVAL, su); // full
    
    if(fork() == 0) {
        // Consumer
        for(int i=0; i<5; i++) {
            sem_wait(semid, 2);
            sem_wait(semid, 0);
            printf("Consumed: %d\n", s->buffer[s->out]);
            s->out = (s->out+1)%BUFFER_SIZE;
            sem_signal(semid, 0);
            sem_signal(semid, 1);
            sleep(1);
        }
        shmdt(s);
        exit(0);
    } else {
        // Producer
        for(int i=1; i<=5; i++) {
            sem_wait(semid, 1);
            sem_wait(semid, 0);
            s->buffer[s->in] = i;
            printf("Produced: %d\n", i);
            s->in = (s->in+1)%BUFFER_SIZE;
            sem_signal(semid, 0);
            sem_signal(semid, 2);
            sleep(1);
        }
        wait(NULL);
        shmdt(s);
        shmctl(shmid, IPC_RMID, NULL);
        semctl(semid, 0, IPC_RMID);
    }
    return 0;
}
