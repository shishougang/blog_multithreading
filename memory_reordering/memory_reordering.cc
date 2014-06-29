#include <pthread.h>
#include <semaphore.h>
#include <cstdio>
#include <cstdlib>

#define USE_CPU_FENCE  0
#define USE_SINGLE_PROCESSOR 0

#if USE_SINGLE_PROCESSOR
#include <sched.h>
#endif

// Mersenne Twister Parameters
#define MT_N 624
#define MT_M 397

/* A thread-safe random number generator

 */
class MersenneTwister {
 public:
  explicit MersenneTwister(int seed);
  unsigned int Integer();

 private:
  unsigned int buffer_[MT_N];
  int index_;
};

MersenneTwister::MersenneTwister(int seed) {
  buffer_[0] = seed;
  for (index_ = 1; index_ < MT_N; ++index_) {
    buffer_[index_] = (1812433253UL * (buffer_[index_-1]
                                       ^ (buffer_[index_-1] >> 30)) + index_);
  }
}

unsigned int MersenneTwister::Integer() {
  if (index_ >= MT_N) {
    unsigned int i;
    unsigned int x;
    for (i = 0; i < MT_N - MT_M; ++i) {
      x = (buffer_[i] & 0x80000000UL) | (buffer_[i+1] & 0x7fffffffUL);
      buffer_[i] = buffer_[i+MT_M] ^ (x >> 1) ^ ((x & 1) * 0x9908b0dfUL);
    }
    for (; i < MT_N - 1; ++i) {
      x = (buffer_[i] & 0x80000000UL) | (buffer_[i+1] & 0x7fffffffUL);
      buffer_[i] = buffer_[i+MT_M-MT_N] ^ (x >> 1) ^ ((x & 1) * 0x9908b0dfUL);
    }
    x = (buffer_[MT_N-1] & 0x80000000UL) | (buffer_[0] & 0x7fffffffUL);
    buffer_[MT_N-1] = buffer_[MT_M-1] ^ (x >> 1) ^ ((x & 1) * 0x9908b0dfUL);
    index_ = 0;
  }
  unsigned int y = buffer_[index_++];
  y ^= (y >> 11);
  y ^= (y << 7) & 0x9d2c5680UL;
  y ^= (y << 15) & 0xefc60000UL;
  y ^= (y >> 18);
  return y;
}

sem_t begin_sem1;
sem_t begin_sem2;
sem_t end_sem;

int X, Y;
int r1, r2;

void *ThreadFunc1(void *param) {
  MersenneTwister random(1);
  for (;;) {
    sem_wait(&begin_sem1);
    // random delay
    while (random.Integer() % 8 != 0) {
    }
    X = 1;
#if USE_CPU_FENCE
    asm volatile("mfence" ::: "memory");  // prevent CPU ordering
#else
    asm volatile("" ::: "memory");  // prevent compiler ordering
#endif
    r1 = Y;
    sem_post(&end_sem);
  }
  return NULL;
}

void *ThreadFunc2(void *param) {
  MersenneTwister random(2);
  for (;;) {
    sem_wait(&begin_sem2);
    // random delay
    while (random.Integer() % 8 != 0) {
    }
    Y = 1;
#if USE_CPU_FENCE
    asm volatile("mfence" ::: "memory"); // prevent CPU ordering
#else
    asm volatile("" ::: "memory");  // prevent compiler ordering
#endif
    r2 = X;
    sem_post(&end_sem);
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  sem_init(&begin_sem1, 0, 0);
  sem_init(&begin_sem2, 0, 0);
  sem_init(&end_sem, 0, 0);

  pthread_t thread[2];
  pthread_create(&thread[0], NULL, ThreadFunc1, NULL);
  pthread_create(&thread[1], NULL, ThreadFunc2, NULL);

#if USE_SINGLE_PROCESSOR
  cpu_set_t cpus;
  CPU_ZERO(&cpus);
  CPU_SET(0, &cpus);
  pthread_setaffinity_np(thread[0], sizeof(cpu_set_t), &cpus);
  pthread_setaffinity_np(thread[1], sizeof(cpu_set_t), &cpus);
#endif

  int detected = 0;
  for (int i = 1; ; ++i) {
    X = 0;
    Y = 0;
    sem_post(&begin_sem1);
    sem_post(&begin_sem2);
    sem_wait(&end_sem);
    sem_wait(&end_sem);
    if (r1 == 0 && r2 == 0) {
      detected++;
      printf("%d reorders detected after %d iterations\n", detected, i);
    }
  }
  return 0;
}

