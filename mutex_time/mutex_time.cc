#include <pthread.h>
#include <ctime>
#include <cstdio>


void GetMonotonicTime(struct timespec *ts) {
  clock_gettime(CLOCK_MONOTONIC, ts);
}

float GetElapsedTime(struct timespec *before, struct timespec *after) {
  double delta_s = after->tv_sec - before->tv_sec;
  double delta_ns = after->tv_nsec - before->tv_nsec;
  return delta_s * 1e9 + delta_ns;
}

pthread_mutex_t lock;

int main(int argc, char *argv[]) {
  pthread_mutex_init(&lock, NULL);
  struct timespec start, end;
  const int kN = 1000000;
  GetMonotonicTime(&start);
  for (int i = 0; i < kN; ++i) {
    pthread_mutex_lock(&lock);
    pthread_mutex_unlock(&lock);
  }
  GetMonotonicTime(&end);
  float delta;
  delta = GetElapsedTime(&start, &end) / kN;
  printf("the average time of lock is %e\n", delta);
  pthread_mutex_destroy(&lock);
  return 0;
}
