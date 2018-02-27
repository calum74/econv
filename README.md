# Efficient entropy conversion

*econv* is a simple C++ library to generate perfect random numbers, and perfect shuffles, directly from hardware entropy.

This algorithm converts entropy very efficiently, which means that very little of the entropy produced by the hardware device is lost in the conversion process. This is interesting from an algorithmic perspective, and practical in case that hardware entropy is a limited resource.

For example, shuffling a deck of 52 cards using `std::shuffle` would read 1632 bits of entropy, losing 1406 bits of hardware entropy each time a deck was shuffled. *econv* loses around 8.6e-15 bits. Similarly, `std::uniform_int_distribution` reads at least 32 bits of entropy to produce a single die throw, losing over 29 bits per output, whereas *econv* would lose just 3.9e-17 bits on average.

## Setup
The entire library consists of a single source file, [entropy_converter.hpp](entropy_converter.hpp).

There is also a test suite and demo, [tests.cpp](tests.cpp), that can be compiled using `g++ tests.cpp --std=c++14` with GCC, or `cl tests.cpp` with Microsoft C++.

Compatibility: C++14. Tested with Visual Studio 2017 and g++ 5.4.

## Example

```c++
#include <entropy_converter.hpp>
#include <random>
#include <algorithm>

entropy_converter<> c;
std::random_device d;

// Generate a uniform number between 1 and 6.
int d6 = c.convert(1,6,d);

// Shuffle a deck of cards
std::vector<int> cards{52};
std::random_shuffle(cards.begin(), cards.end(), c.with_generator(d));
```

## Reference

```c++
template<typename T=unsigned, typename U=unsigned>
class entropy_converter<T, U>;
```

### Types
```c++
typedef T result_type;
```
The datatype used to store non-binary entropy. The larger this data type, the more efficient the entropy conversion becomes, but the entropy converted would pre-fetch more entropy, up to the size of `result_type`.

```c++
typedef U buffer_type;
```
The datatype used to store binary entropy.  It must be at least as large as the binary input device, and defaults to `unsigned` which is the size of data produced by `std::random_device`.

### Constructors

`entropy_converter()`
Initializes the entropy converter.

### `convert` method

```
template<typename Generator>
result_type convert(result_type target, Generator & gen)

template<typename Result, typename Generator>
Result convert(Result outMin, Result outMax, Generator & gen)

template<typename Result, typename Input, typename Generator>
Result convert(Result outMin, Result outMax, Input inMin, Input inMax, Generator & gen,
               result_type limit = std::numeric_limits<result_type>::max())
```
Returns a uniform random number in the specified range. Either between `0` and `target-1` or between `outMin` and `outMax` inclusive.

`gen` is a random number engine, producing a uniform random number between `inMin` and `inMax`. If `inMin` and `inMax` are not specified, then `gen.min()` and `gen.max()` are used.

`gen` must be callable with `operator()`, and may be called zero or more times. When the input range is a power of 2, it must not exceed the capacity of `buffer_type`. When the input range is not a power of 2, then the product of the input and output ranges must not exceed the capacity of `result_type`. These conditions are checked in debug mode, but not in release mode.

`limit` controls the size of buffered entropy but there is normally no need to specify this.

### Convenience methods

```c++
template<typename Generator>
auto with_generator(Generator &gen)
```
Returns a functor taking an integer argument `x`, that returns a uniform random numer between 0 and `x-1`.

Example: `std::random_shuffle(cards.begin(), cards.end(), c.with_generator(d));`

```c++
template<typename Result>
auto make_uniform(Result a, Result b)
```
Returns a functor taking a generator, and returns a uniform random number between `a` and `b`.

```c++
template<typename Result, typename Generator>
auto make_uniform(Result a, Result b, Generator &gen)
```
Returns a functor taking no arguments, and returns a uniform random number between `a` and `b`.

## Theoretical background
Producing uniform random numbers from a hardware source presents two challenges:

1) Ensuring that the resulting number is perfectly uniform
2) Avoiding losing too much entropy in the conversion process.

The simple solution, `rand()%n`, is wrong because the resulting distribution will always be biased, except when n is itself a power of 2. In fact any algorithm using a finite quantity of binary entropy results in a biased distribution, and the only correct solution is to allow for an unbounded amount of input entropy.

A correct solution is something like

```
do:
    x = fetch_some_entropy()
while x>=n
```

Whilst this is correct, it is not very efficient in terms of its entropy conversion. It loses entropy for three reasons. Firstly, the algorithm may loop several times, and entropy is lost on each iteration. Secondly, the condition `x>=n` itself destroys entropy, and thirdly, the residual entropy in `x` is thrown away each iteration.

*econv* uses a modified algorithm that addresses these problems:

```
init():
    value=0
    range=1

int fetch(int base):
    while range<c/base:
        value = value * base + read_entropy_from device(base)
        range  = range * base

int convert(int n, int base=2):
	do:
		fetch(base)
		kn = range - range%n
		if value < kn:
			result = value % n
			value = value/n
			range = kn/n
			return result
		else
			value -= kn
			range -= kn
	loop
```
The algorithm stores entropy in the variable `value`, which is a uniform random number between `0` and `range-1`. Initially, `value` contains no entropy, but as soon as `convert` is called, the algorithm reads as much entropy as possible from the outside source into the `value`, up to a maximum of `c`. In general, `value` will contain entropy from the previous iteration, but the invariant is that value is a uniform random variable between `0` and `range-1`.

Next, the algorithm find the highest multiple of `n`, `kn`, smaller than `range`, by subtracting the modulus. If `value<kn`, then we know that `value` is a uniform random variable less than `kn`, so we can factorize the uniform random variable into two parts: `k` and `n`. We return a random variable of size `n`, and the remaining entropy is stored in `value` for the next time `convert` is called.

If `value<kn` is false, then value lies between `kn` and `range-1`. Thus we subtract `kn` from both `value` and `range`, preserving our invariant that `value` is between `0` and `range-1`.

## Analysis
The only place this algorithm loses entropy is in the comparison `value<kn`, which yields a smaller random variable in both cases. The amount of entropy lost by this comparison is given by the [binary entropy function] (https://en.wikipedia.org/wiki/Binary_entropy_function)

Entropy loss per comparison = `-plgp - (1-p)lg(1-p)`

Here, `p` is the probability of `value<kn`

    p = P(value<kn) = kn/value = (range - range%n) / range > 1-n/range > 1-2n/c

The expected number of times we go round the loop is `1/p`.

Thus the expected entropy loss of this algorithm =

        (-plgp-(1-p)lg(1-p))/p 

We now see the purpose of fetching as much entropy as possible up front. It means that the entropy loss is tiny. For example if n=6 and c=2^64,  then n/C ~= 6.5e-19. This makes the entropy loss around 3.9e-17 bits per conversion.

The `fetch()` function can be changed to fetch entropy of a different base `m`. In that case, `p > 1+nm/c`, which is in general very small (and therefore efficient), unless `n` or `m` are very large.

It follows that the entropy loss is governed by the ration `n/c`, and the larger c, the more efficient the entropy conversion. For example, `entropy_converter<uint64_t>` is more efficient than `entropy_converter<uint32_t>`, at the expense of slightly more buffering.

If the input to *econv* is perfect, then its output will be perfectly distributed independent random numbers. *econv* can be used as a normal uniform random number generator to supply uniform random numbers to a shuffling algorithm like Fisher-Yates. Since the random numbers are perfect, then the result of the shuffle will be perfect as well.
