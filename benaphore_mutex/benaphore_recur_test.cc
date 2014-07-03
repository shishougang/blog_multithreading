#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <ctime>
#include <cstdio>

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


// Mersenne Twister Parameters
#define MT_N 624
#define MT_M 397

class MersenneTwister {
 public:
  explicit MersenneTwister(int seed);
  unsigned int Integer();
  float fraction() {
    return Integer() * 2.3283e-10f;
  }

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

struct ThreadStats {
  int iterations;
  int work_units_complete;
  int amount_incremented;

  ThreadStats() {
    iterations = 0;
    work_units_complete = 0;
    amount_incremented = 0;
  }

  void Accumulate(const ThreadStats &rhs) {
    iterations += rhs.iterations;
    work_units_complete += rhs. work_units_complete;
    amount_incremented += rhs.amount_incremented;
  }
};


const int kMaxThreads = 4;
ThreadStats g_thread_stats[kMaxThreads];

RecursiveBenaphore g_lock;

int g_counter = 0;
bool g_done = false;

void *ThreadProc(void *param) {
  ThreadStats local_state;
  int thread_number = *(static_cast<int *>(param));
  MersenneTwister random(thread_number);
  int lock_count = 0;
  int last_counter = 0;
  LIGHT_ASSERT(thread_number < kMaxThreads);
  for (;;) {
    local_state.iterations++;
    float f = random.fraction();
    int work_units = static_cast<int>(f * 10);
    for (int i = 0; i < work_units; ++i) {
      random.Integer();
    }
    local_state.work_units_complete += work_units;

    if (lock_count > 0) {
      LIGHT_ASSERT(g_counter == last_counter);
    }

    // the number of desired lock count in rang [0, 4), biased to low numbers
    f = random.fraction();
    int desired_lock_count = static_cast<int>(f * f * 4);
    while (lock_count > desired_lock_count) {
      g_lock.Unlock();
      lock_count--;
    }
    bool use_trylock = (random.Integer() & 1) == 0;
    while (lock_count < desired_lock_count) {
      if (use_trylock) {
        if (!g_lock.TryLock()) {
          break;
        }
      } else {
        g_lock.Lock();
      }
      lock_count++;
    }

    if (lock_count > 0) {
      LIGHT_ASSERT((g_counter - last_counter) >= 0);
      g_counter += thread_number + 1;
      last_counter = g_counter;
      local_state.amount_incremented += thread_number + 1;
    }

    if (g_done) {
      break;
    }
  }
  while (lock_count > 0) {
    g_lock.Unlock();
    lock_count--;
  }
  g_thread_stats[thread_number] = local_state;
  return NULL;
}

void PerformStressTest(int thread_count, bool use_affinities,
                       int milliseconds) {
  g_counter = 0;
  g_done = false;
  printf("Spawning %d threads %s affinites for %d milliseconds.\n",
         thread_count, use_affinities? "with" : "without", milliseconds);
  pthread_t threads[kMaxThreads];
  int thread_ids[kMaxThreads];
  for (int t = 0; t < thread_count; ++t) {
    thread_ids[t] = t;
    int rc;
    rc = pthread_create(&threads[t], NULL,
                        ThreadProc, &thread_ids[t]);
    if (rc) {
      fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
      return;
    }
    if (use_affinities) {
      cpu_set_t cpus;
      CPU_ZERO(&cpus);
      CPU_SET(t, &cpus);
      pthread_setaffinity_np(threads[t], sizeof(cpu_set_t), &cpus);
    }
  }
  usleep(milliseconds * 1000);
  g_done = true;

  for (int t = 0; t < thread_count; ++t) {
    pthread_join(threads[t], NULL);
  }

  ThreadStats total_stats;
  for (int t = 0; t < thread_count; ++t) {
    total_stats.Accumulate(g_thread_stats[t]);
  }
  LIGHT_ASSERT(total_stats.amount_incremented == g_counter);
  printf("%d total iterations, %d workUnits, g_counter=%d\n",
         total_stats.iterations, total_stats.work_units_complete, g_counter);
}


int main(int argc, char *argv[]) {
  for (int iterations = 0; iterations < 10; ++iterations) {
    for (int affinities = 0; affinities < 2; ++affinities) {
      for (int thread_count = 2; thread_count <= kMaxThreads; ++thread_count) {
        PerformStressTest(thread_count, affinities == 0, 2000);
      }
    }
  }
  return 0;
}
