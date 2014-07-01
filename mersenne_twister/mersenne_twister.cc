#include <cmath>
#include <iostream>
using namespace std;
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

int main(int argc, char *argv[]) {
  MersenneTwister prng(1234);
  int i;
  for (i = 0; i < 10; ++i) {
    cout << prng.Integer() << endl;
  }
  cout << endl;
  double sum = 0;
  for (i = 0; i < 100000; ++i) {
    sum += prng.PoissonInterval(40);
  }
  cout << sum / i  << endl;
  return 0;
}
