#include <random>
#include <vector>
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/shm.h>

#define M 5 // Количество рядов
#define N 2 // Количество шкафов в ряду
#define K 3 // Количество книг в шкафе
const char* shmemory_name = "/shared_memory";
struct SharedMemory {
    int catalog[M][N][K];
};
int main() {
    std::random_device rd;
    std::mt19937 g(rd());
    std::vector<int> v(M * N * K);
    std::iota(v.begin(), v.end(), 1);
    std::shuffle(v.begin(), v.end(), g);
    for (int m = 0; m < M; ++m) {
        for (int n = 0; n < N; ++n) {
            for (int k = 0; k < K; ++k) {
                std::cout << "Книга " << v[m * N * K + n * K + k] << " находится в " << m + 1 << " ряду, " << n + 1 << " шкафу, на " << k + 1 << " месте\n";
            }
        }
    }
    int shmid = shm_open(shmemory_name, O_CREAT | O_RDWR, 0666);
    if (shmid == -1) {
        perror("Error creating shared memory");
        exit(-1);
    }
    ftruncate(shmid, sizeof(SharedMemory));
    SharedMemory* sharedData = static_cast<SharedMemory*>(mmap(nullptr, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0));

    const char* shar_object = "/posix-shar-object";
    int shmid_sem;
    if ((shmid_sem = shm_open(shar_object, O_CREAT | O_RDWR, 0666)) == -1) {
        perror("shm_open");
        exit(-1);
    }
    if (ftruncate(shmid_sem, sizeof (sem_t)) == -1) {
        perror("ftruncate: count memory sizing error");
        exit(-1);
    }
    sem_t* p_sem = static_cast<sem_t*>(mmap(0, sizeof (sem_t), PROT_WRITE|PROT_READ, MAP_SHARED, shmid_sem, 0));
    if (sem_init(p_sem, 1, 1) == -1) {
        perror("Error creating semaphore");
        exit(-1);
    }
    for (int m = 0; m < M; ++m) {
        pid_t childPid = fork();
        if (childPid == 0) {
            if (sem_wait(p_sem) == -1) {
                perror("sem_wait: Incorrect wait of posix semaphore");
                exit(-1);
            };
            for (int n = 0; n < N; ++n) {
                for (int k = 0; k < K; ++k) {
                    sharedData->catalog[m][n][k] = v[m * N * K + n * K + k];
                }
            }
            if (sem_post(p_sem) == -1) {
                perror("sem_post: Incorrect post of posix semaphore");
                exit(-1);
            };
            exit(0);
        } else if (childPid == -1) {
            perror("Error creating child process");
            exit(-1);
        }
    }
    for (int m = 0; m < M; ++m) {
        wait(0);
    }
    for (int m = 0; m < M; ++m) {
        std::cout << "Ряд " << m + 1 << ":\n";
        for (int n = 0; n < N; ++n) {
            std::cout << "Шкаф " << n + 1 << ": ";
            for (int k = 0; k < K; ++k) {
                std::cout << sharedData->catalog[m][n][k] << ' ';
            }
            std::cout << '\n';
        }
    }
    sem_destroy(p_sem);
    shm_unlink(shar_object);
    shm_unlink(shmemory_name);
    return 0;
}
