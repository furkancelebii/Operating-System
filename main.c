#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define TAMPON_BOYUTU 10
#define NUM_THREADS 10

sem_t dolu, bos;
pthread_mutex_t kilit;

int tampon[TAMPON_BOYUTU];
int tampon_index = 0;

int sayac = 1; // Paylaþýlan sayac deðiþkeni

int shm_id; // Paylaþýlan bellek id
int* shared_memory; // Paylaþýlan bellek iþaretcisi

int pipe_fd[2]; // Pipe dosya tanýmlayýcýlarý

void* uretici(void* arg) {
    while (1) {
        sem_wait(&bos);
        pthread_mutex_lock(&kilit);

        if (tampon_index < TAMPON_BOYUTU) {
            tampon[tampon_index++] = sayac;
            printf("Üretici: %d üretildi\n", sayac++);
            if (sayac > 10) sayac = 1;
        }

        pthread_mutex_unlock(&kilit);
        sem_post(&dolu);
        sleep(1);
    }
    return NULL;
}

void* tuketici(void* arg) {
    while (1) {
        sem_wait(&dolu);
        pthread_mutex_lock(&kilit);

        if (tampon_index > 0) {
            printf("Tüketici: %d tüketildi\n", tampon[--tampon_index]);
        }

        pthread_mutex_unlock(&kilit);
        sem_post(&bos);
        sleep(1);
    }
    return NULL;
}

int main() {
    pthread_t uretici_thread, tuketici_threads[NUM_THREADS];

    pthread_mutex_init(&kilit, NULL);
    sem_init(&dolu, 0, 0);
    sem_init(&bos, 0, TAMPON_BOYUTU);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&tuketici_threads[i], NULL, tuketici, NULL);
    }

    if (pipe(pipe_fd) == -1) {
        perror("Pipe oluþturma baþarýsýz");
        exit(EXIT_FAILURE);
    }

    shm_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("Paylaþýlan bellek oluþturma baþarýsýz");
        exit(EXIT_FAILURE);
    }

    shared_memory = (int*)shmat(shm_id, NULL, 0);
    if (shared_memory == (int*)(-1)) {
        perror("Paylaþýlan bellek ekleme baþarýsýz");
        exit(EXIT_FAILURE);
    }

    pid_t child_pid = fork();

    if (child_pid == -1) {
        perror("Çocuk iþlem oluþturma hatasý");
        exit(EXIT_FAILURE);
    } else if (child_pid == 0) {
        // Çocuk iþlem: Pipe ile veri okuma
        close(pipe_fd[1]); // Yazma tarafýný kapat
        tuketici(NULL); // Argüman boþ geçiliyor, gereksiz
        exit(EXIT_SUCCESS);
    } else {
        // Ana iþlem: Üretici iþ parçacýðý
        close(pipe_fd[0]); // Okuma tarafýný kapat

        pthread_create(&uretici_thread, NULL, uretici, NULL);

        pthread_join(uretici_thread, NULL);

        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(tuketici_threads[i], NULL);
        }

        wait(NULL); // Çocuk iþleminin bitmesini bekleyin

        pthread_mutex_destroy(&kilit);
        sem_destroy(&dolu);
        sem_destroy(&bos);

        shmdt(shared_memory);
        shmctl(shm_id, IPC_RMID, NULL);

        close(pipe_fd[1]);

        return 0;
    }
}

