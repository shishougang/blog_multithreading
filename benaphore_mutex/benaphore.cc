#include <semaphore.h>
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


class Benaphore {
 public:
  Benaphore() : counter_(0) {
    sem_init(&semaphore_, 0, 0);
  }
  ~Benaphore() {
    sem_destroy(&semaphore_);
  }
  void Lock() {
    if (__sync_add_and_fetch(&counter_, 1) > 1) {
      sem_wait(&semaphore_);
    }
  }
  void Unlock() {
    if (__sync_sub_and_fetch(&counter_, 1) > 0) {
      sem_post(&semaphore_);
    }
  }
  bool TryLock() {
    return __sync_bool_compare_and_swap(&counter_, 0, 1);
  }
    
 private:
  long counter_;
  sem_t semaphore_;
};

int main(int argc, char *argv[]) {
  struct timespec start, end;
  const int kN = 1000000;
  Benaphore benaphore;
  GetMonotonicTime(&start);
  for (int i = 0; i < kN; ++i) {
    benaphore.Lock();
    benaphore.Unlock();
  }
  GetMonotonicTime(&end);
  float delta;
  delta = GetElapsedTime(&start, &end) / kN;
  printf("the average time of lock is %e\n", delta);
  return 0;
}
