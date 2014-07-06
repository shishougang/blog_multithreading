#include <semaphore.h>
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


#define LIGHT_ASSERT(x) { if (!(x)) __builtin_trap(); }

class RecursiveBenaphore {
 public:
  RecursiveBenaphore() : counter_(0), owner_(0), recursion_(0) {
    sem_init(&semaphore_, 0, 0);
  }
  ~RecursiveBenaphore() {
    sem_destroy(&semaphore_);
  }
  void Lock() {
    pthread_t thread_id = pthread_self();
    if (__sync_add_and_fetch(&counter_, 1) > 1) {
      if (!pthread_equal(thread_id, owner_)) {
        sem_wait(&semaphore_);
      }
    }
    owner_ = thread_id;
    recursion_++;
  }
  void Unlock() {
    pthread_t thread_id = pthread_self();
    LIGHT_ASSERT(pthread_equal(thread_id, owner_));
    long recur = --recursion_;
    if (recur == 0) {
      owner_ = 0;
    }
    long result = __sync_sub_and_fetch(&counter_, 1);
    if (result > 0) {
      if (recur == 0) {
        int sem_value;
        sem_getvalue(&semaphore_, &sem_value);
        if (sem_value == 0) {
          sem_post(&semaphore_);
        }
      }
    }
  }
  bool TryLock() {
    pthread_t thread_id = pthread_self();
    if (pthread_equal(thread_id, owner_)) {
      __sync_add_and_fetch(&counter_, 1);
    } else {
      bool result = __sync_bool_compare_and_swap(&counter_, 0, 1);
      if (result == false) {
        return false;
      }
      owner_ = thread_id;
    }
    recursion_++;
    return true;
  }

 private:
  long counter_;
  sem_t semaphore_;
  pthread_t owner_;
  long recursion_;
};


int main(int argc, char *argv[]) {
  struct timespec start, end;
  const int kN = 1000000;
  RecursiveBenaphore benaphore;
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
