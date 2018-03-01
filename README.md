# Efficient entropy conversion

## Overview
_econv_ is an easy to use C++ library to generate perfect random numbers, and perfect shuffles, directly from hardware entropy.

This algorithm converts entropy very efficiently, which means that very little of the entropy produced by the hardware device is lost in the conversion process. This is interesting from an algorithmic perspective, and practical in case that hardware entropy is a limited resource.

For example, shuffling a deck of 52 cards using `std::shuffle` would read 1632 bits of entropy, losing 1406 bits of hardware entropy each time a deck was shuffled. *econv* loses around 8.6e-15 bits. Similarly, `std::uniform_int_distribution` reads at least 32 bits of entropy to produce a single die throw, losing over 29 bits per output, whereas *econv* would lose just 3.9e-17 bits on average.

## Setup
The entire library can be distributed as a single source file, [entropy_converter.hpp](entropy_converter.hpp).

There is also a test suite and demo, [tests.cpp](tests.cpp), that can be compiled using `g++ tests.cpp --std=c++14` with GCC, or `cl tests.cpp` with Microsoft C++.

Compatibility: C++14. Tested with Visual Studio 2017 and g++ 5.4.

## Usage

The header file `entropy_converter.hpp` provides the `entropy_converter` class. As there is only one class, it is not in a namespace.

```c++
#include <entropy_converter.hpp>
```

The purpose of `entropy_converter` is to convert uniform random numbers from one range of values to another. Typically, `entropy_converter` reads entropy from `std::random_device`, and outputs uniform random numbers in a specified range, or can be used to shuffle an array.

The input and output range do not need to be fixed, and the input range does not need to be a power of 2.

In the following example, the `convert` function is used to read entropy from `std::random_device` and stores the result in the variable `die_roll`.

```c++
#include <entropy_converter.hpp>
#include <random>  // For std::random_device

entropy_converter<> c;
std::random_device d;

int die_roll = c.convert(1,6,d);
```

`entropy_converter` buffers entropy, since normally the input provides more entropy than is actually needed. `std::random_device` usually produces entropy in chunks of 32 bits. To ensure efficiency, it is important to use the same `entropy_converter` and not destroy it. Ideally, there would only be one instance of `entropy_converter`.

Write

```c++
entropy_converter<> c;	// Do this
for(int i=0; i<1000; ++i)
	std::cout << c.convert(1,6,d);
```

and not

```c++
for(int i=0; i<1000; ++i)
{
	entropy_converter<> c;	// Do not do this
	std::cout << c.convert(1,6,d);
}
```

To shuffle an array, use the function `std::random_shuffle()`, which implements a perfect shuffle using the Fisher-Yates algorithm. The `with_generator()` method returns a functor that can be used by `std::random_shuffle()`. If the input source is perfectly uniform, then `entropy_converter` combined with `std::random_shuffle` produce theoretically perfect shuffles.

For example,

```c++
#include <algorithm>   // For std::random_shuffle

std::vector<int> cards{52};

std::random_shuffle(cards.begin(), cards.end(), c.with_generator(d));
```

## Reference

### Declaration

```c++
template<typename T=unsigned, typename U=unsigned>
class entropy_converter<T, U>
```
See the next section for a description of the type parameters `T` and `U`.

### Member types
```c++
typedef T result_type;
```
The datatype used to store non-binary entropy. The larger this data type, the more efficient the entropy conversion becomes, but the entropy converter would pre-fetch more entropy up to the size of `result_type`.

To ensure good efficiency, the size of `result_type` should be much larger than the output range, and *must* be twice as large as the maximum output range required.

```c++
typedef U buffer_type;
```
The datatype used to store binary entropy.  It must be at least as large as the binary input device, and defaults to `unsigned` which is the size of data produced by `std::random_device`.

### Constructors

```c++
entropy_converter();
entropy_converter(entropy_converter&&);
entropy_converter(const entropy_converter&) = delete;
```
Initializes the entropy converter. The copy constructor is deleted as it is not permitted to clone entropy and is almost certainly a mistake.

### `operator =`

```c++
entropy_converter& operator=(entropy_converter&&);
entropy_converter& operator=(const entropy_converter&) = delete;
```
Moves the internal entropy buffer from another `entropy_converter`. The copy operator is deleted as it is almost certainly a mistake.

### `convert()`

```
template<typename Generator>
result_type convert(result_type target, Generator & gen)

template<typename Result, typename Generator>
Result convert(Result outMin, Result outMax, Generator & gen)

template<typename Result, typename Input, typename Generator>
Result convert(Result outMin, Result outMax, Input inMin, Input inMax, Generator & gen,
               result_type limit = std::numeric_limits<result_type>::max())
```
This method reads uniform integers from `gen` and returns uniform integers in the specified range.

The output range is either between `0` and `target-1` or between `outMin` and `outMax` inclusive.

`gen` is a functor that returns an input value in the input range. It is compatible with C++ random number engines. The input range is defined implicitly by `gen.min()` and `gen.max()`, or can be explicitly supplied as arguments `inMin` and `inMax`.

`limit` controls the size of buffered entropy but there is normally no need to specify this as it is generally advantageous to buffer as much entropy as possible.

If the input range is a power of 2, then the input range must be represented by `buffer_type`, and the output range must be no more than `limit/2`. If the input range is not a power of 2, then the product of the input and output ranges must not exceed `limit`. These constraints can be changed by specifying a different `result_type` and `buffer_type` as template parameters to `entropy_converter`.

`convert()` uses constant time and memory. It does not allocate any memory.

Exceptions: `convert()` is exception neutral to `gen` throwing exceptions. If `gen()`, `gen.max()` or `gen.min()` throw an exception, then it is passed through `convert()`.

Specifying an invalid input or output range throws `std::range_error`.

Exceptions do not lose entropy or invalidate the internal state of `entropy_converter`.

### Convenience methods

```c++
template<typename Generator>
auto with_generator(Generator &gen)
```
Returns a functor taking an integer argument `x` returning a uniform random number between 0 and `x-1`.

Example: `std::random_shuffle(cards.begin(), cards.end(), c.with_generator(d));`

```c++
template<typename Result>
auto make_uniform(Result a, Result b)
```
Returns a functor taking a generator returning a uniform random number between `a` and `b`.

```c++
template<typename Result, typename Generator>
auto make_uniform(Result a, Result b, Generator &gen)
```
Returns a functor taking no arguments returning a uniform random number between `a` and `b`.

### Thread safety

`entropy_converter` is not synchronised and is not intended to be used concurrently. Concurrent access to an `entropy_converter` is undefined. In practise, it is probably a good idea to have one `entropy_converter` instance per thread.

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
The only place this algorithm loses entropy is in the comparison `value<kn`, which yields a smaller random variable in both cases. The amount of entropy lost by this comparison is given by the [binary entropy function](https://en.wikipedia.org/wiki/Binary_entropy_function).

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
