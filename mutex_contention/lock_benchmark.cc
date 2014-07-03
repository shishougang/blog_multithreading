#include <pthread.h>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <algorithm>
using std::min;

typedef long long int uint64_t;
// Mersenne Twister Parameters
#define MT_N 624
#define MT_M 397

class MersenneTwister {
 public:
  explicit MersenneTwister(int seed);
  unsigned int Integer();
  float PoissonInterval(float rate) {
    return -logf(1.0f - Integer() * 2.3283e-10f) * rate;
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

static const int kMaxThreads = 4;

struct ThreadStats {
  uint64_t workdone;
  uint64_t iterations;
  uint64_t overshoot;
};

struct GlobalState {
  pthread_mutex_t thread_mutex;
  int count;
  pthread_mutex_t count_mutex;
  pthread_cond_t count_cond;
  float secs_per_work_unit;
  float time_limit;
  float average_unlock_count;
  float average_locked_count;
  ThreadStats thread_stats[kMaxThreads];
};

GlobalState global_state = {0};

struct BenchmarkParams {
  int thread_count;
  float lock_interval;  // In seconds
};

BenchmarkParams g_benchmark_params[] = {
  // Reference
    1, 10e-3f,      // 10 ms        100/s 

    // Test 15000 locks per second with multiple threads
    /*1, 1/15000.0f,
    2, 1/15000.0f,
    3, 1/15000.0f,
    4, 1/15000.0f,*/
    
    // Test various lock rates with 2 threads
    2, 10e-9f,      // 10 ns        100000000/s
    2, 31.6e-9f,    // 31.6 ns      31600000/s
    2, 100e-9f,     // 100 ns       10000000/s
    2, 316e-9f,     // 316 ns       3160000/s
    2, 1e-6f,       // 1 us         1000000/s
    2, 3.16e-6f,    // 3.16 us      316000/s
    2, 10e-6f,      // 10 us        100000/s
    2, 31.6e-6f,    // 31.6 us      31600/s
    2, 100e-6f,     // 100 us       10000/s
    
};

void GetMonotonicTime(struct timespec *ts) {
  clock_gettime(CLOCK_MONOTONIC, ts);
}

float GetElapsedTime(struct timespec *before, struct timespec *after) {
  double delta_s = after->tv_sec - before->tv_sec;
  double delta_ns = after->tv_nsec - before->tv_nsec;
  return delta_s + delta_ns * 1e-9;
}

void* ThreadProc(void *arg) {
  // Initialize
  int thread_number = *(static_cast<int*>(arg));
  MersenneTwister random(thread_number);
  struct timespec start, end;
  float elapsed_time = 0;
  ThreadStats thread_stats = {0};
  int work_units = 0;

  // Indicate ready, wait for start
  pthread_mutex_lock(&global_state.count_mutex);
  global_state.count++;
  pthread_cond_broadcast(&global_state.count_cond);
  pthread_mutex_unlock(&global_state.count_mutex);

  pthread_mutex_lock(&global_state.count_mutex);
  while (global_state.count != 0) {
    pthread_cond_wait(&global_state.count_cond,
                      &global_state.count_mutex);
  }
  pthread_mutex_unlock(&global_state.count_mutex);
  GetMonotonicTime(&start);
  for (;;) {
    work_units = static_cast<int> (random.PoissonInterval(
        global_state.average_unlock_count) + 0.5f);
    for (int i = 0; i < work_units; ++i) {
      random.Integer();
    }
    thread_stats.workdone += work_units;

    GetMonotonicTime(&end);
    elapsed_time = GetElapsedTime(&start, &end);
    if (elapsed_time >= global_state.time_limit) {
      break;
    }

    // Do some work while holding the lock
    pthread_mutex_lock(&global_state.thread_mutex);
    work_units = static_cast<int> (random.PoissonInterval(
        global_state.average_locked_count) + 0.5f);
    for (int i = 0; i < work_units; ++i) {
      random.Integer();
    }
    thread_stats.workdone += work_units;
    pthread_mutex_unlock(&global_state.thread_mutex);

    thread_stats.iterations++;
    GetMonotonicTime(&end);
    elapsed_time = GetElapsedTime(&start, &end);
    if (elapsed_time >= global_state.time_limit) {
      break;
    }
  }

  // Estimate the number of work units which went over the time limit
  thread_stats.overshoot = min(work_units,
                               static_cast<int>
                               ((elapsed_time -global_state.time_limit) /
                                     global_state.secs_per_work_unit));
  global_state.thread_stats[thread_number] = thread_stats;
  return NULL;
}


float CalcSecsPerWorkUnit() {
  MersenneTwister random(1234);
  struct timespec start, end;
  int count = 100000000;
  GetMonotonicTime(&start);
  for (int i = 0; i < count; ++i) {
    random.Integer();
  }
  GetMonotonicTime(&end);
  float elapsed_time = GetElapsedTime(&start, &end);
  return elapsed_time / count;
}

int main(int argc, char *argv[]) {
  pthread_mutex_init(&global_state.thread_mutex, NULL);
  pthread_mutex_init(&global_state.count_mutex, NULL);
  global_state.count = 0;
  pthread_cond_init(&global_state.count_cond, NULL);
  global_state.time_limit = 1.0f;
  global_state.secs_per_work_unit = CalcSecsPerWorkUnit();
  printf("secsPerWorkUnit = %e\n", global_state.secs_per_work_unit);
  for (int b = 0; b <
           sizeof(g_benchmark_params) / sizeof(g_benchmark_params[0]); ++b) {
    float avg_work_units_between_locks = g_benchmark_params[b].lock_interval /
        global_state.secs_per_work_unit;
    int thread_count = g_benchmark_params[b].thread_count;
    static const int kSteps = 200;
    for (int s = 0; s < kSteps; ++s) {
      global_state.count = 0;
      global_state.average_locked_count = avg_work_units_between_locks *
          s / kSteps;
      global_state.average_unlock_count = avg_work_units_between_locks *
          (kSteps - s) / kSteps;
      pthread_t threads[kMaxThreads];
      int thread_ids[kMaxThreads];
      for (int t = 0; t < thread_count; ++t) {
        thread_ids[t] = t;
        int rc;
        if ((rc = pthread_create(&threads[t], NULL,
                                 ThreadProc, &thread_ids[t]))) {
          fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
          return -1;
        }

        cpu_set_t cpus;
        CPU_ZERO(&cpus);
        CPU_SET(t, &cpus);
        pthread_setaffinity_np(threads[t], sizeof(cpu_set_t), &cpus);
      }

      // Wait all the threads are ready
      pthread_mutex_lock(&global_state.count_mutex);
      while (global_state.count != thread_count) {
        pthread_cond_wait(&global_state.count_cond,
                          &global_state.count_mutex);
      }
      pthread_mutex_unlock(&global_state.count_mutex);

      // Start threads
      pthread_mutex_lock(&global_state.count_mutex);
      global_state.count = 0;
      pthread_cond_broadcast(&global_state.count_cond);
      pthread_mutex_unlock(&global_state.count_mutex);
      for (int t = 0; t < thread_count; ++t) {
        pthread_join(threads[t], NULL);
      }

      // Report
      printf("threads=%d ", thread_count);
      printf("lockInterval=%e ", g_benchmark_params[b].lock_interval);
      printf("lockDuration=%f ", (s * 1.0 / kSteps));
      ThreadStats totals = {0};
      for (int t = 0; t < thread_count; ++t) {
        totals.workdone += global_state.thread_stats[t].workdone;
        totals.iterations += global_state.thread_stats[t].iterations;
        totals.overshoot += global_state.thread_stats[t].overshoot;
      }
      printf("workDone=%llu ", totals.workdone);
      printf("iteratons=%llu ", totals.iterations);
      printf("overshoot=%llu \n", totals.overshoot);
    }
  }
  pthread_mutex_destroy(&global_state.thread_mutex);
  pthread_mutex_destroy(&global_state.count_mutex);
  pthread_cond_destroy(&global_state.count_cond);
  return 0;
}
