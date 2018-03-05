# Efficient entropy conversion

## Overview
_econv_ is an easy to use C++ library to generate perfect random numbers, and perfect shuffles, directly from a hardware entropy source.

The algorithm converts entropy very efficiently, which means that very little entropy is lost in the conversion process. This is interesting from an algorithmic perspective, and practical in case that hardware entropy is a limited resource.

For example, shuffling a deck of 52 cards using `std::shuffle` reads 1632 bits of entropy, losing 1406 bits of entropy each time. _econv_ loses around 8.6e-15 bits. Similarly, `std::uniform_int_distribution` reads at least 32 bits of entropy to produce a single die throw, losing over 29 bits, whereas *econv* would lose on average just 3.9e-17 bits.

## Setup
The entire library consists of a single header file, [entropy_converter.hpp](entropy_converter.hpp), that can simply by copied to the desired location.

The test suite and demo, [tests.cpp](tests.cpp), can be compiled using `g++ tests.cpp --std=c++14` with GCC, or `cl tests.cpp` with Microsoft C++.

Compatibility: C++14. Tested with Visual Studio 2017, clang 9.0 and g++ 5.4.

## Quick guide

The header file `entropy_converter.hpp` provides the `entropy_converter` class. As there is only one class, it is not in a namespace.

```c++
#include <entropy_converter.hpp>
```

The purpose of `entropy_converter` is to convert uniform random numbers from one range of values to another. Typically, `entropy_converter` reads entropy from `std::random_device`, and outputs uniform random numbers in a specified range, or can be used with `std::random_shuffle()` to shuffle an array.

The input and output ranges can be variable, and the input range does not need to be a power of 2.

In the following example, the `convert` function is used to read entropy from `std::random_device` and returns a random number between 1 and 6.

```c++
#include <entropy_converter.hpp>
#include <random>  // For std::random_device
#include <iostream>

int main()
{
    entropy_converter<> c;
    std::random_device d;
    std::cout << c.convert(1,6,d) << std::endl;
}
```

`entropy_converter` buffers entropy between invocations of `convert()`. To ensure efficiency, it is important to reuse the same instance of `entropy_converter`. For example, write

```c++
entropy_converter<> c;  // Do this
for(int i=0; i<1000; ++i)
    std::cout << c.convert(1,6,d);
```

and not

```c++
for(int i=0; i<1000; ++i)
{
    entropy_converter<> c;  // Do not do this
    std::cout << c.convert(1,6,d);
}
```

To shuffle an array, use the function `std::random_shuffle()`, which implements a perfect shuffle using the [Fisher-Yates algorithm](https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle). The `with_generator()` method returns a functor that can be used by `std::random_shuffle()`. If the input source is perfectly uniform, then `entropy_converter` combined with `std::random_shuffle` produce theoretically perfect shuffles.

The following example outputs a random deck of cards:

```c++
#include <entropy_converter.hpp>
#include <random>
#include <algorithm>   // For std::random_shuffle
#include <numeric>
#include <iostream>

int main()
{
    entropy_converter<> c;
    std::random_device d;
    std::vector<int> cards(52);
    std::iota(cards.begin(), cards.end(), 0);
    std::random_shuffle(cards.begin(), cards.end(), c.with_generator(d));
    for(auto card : cards)
        std::cout << "A23456789TJQK"[card%13] << "SHCD"[card/13] << " ";
    std::cout << std::endl;
}
```

## Reference

### Declaration

```c++
template<typename T=unsigned, typename U=unsigned>
class entropy_converter<T, U>
```

### Member types
```c++
typedef T result_type;
```
The datatype used to convert output entropy. The larger this data type, the more efficient the entropy conversion, but the entropy converter pre-fetches more entropy up to the size of `result_type`.

To ensure good efficiency, the size of `result_type` should be much larger than the output range, and *must* be at least twice as large as the maximum output range required.

```c++
typedef U buffer_type;
```
The datatype used to buffer binary entropy.  It must be large enough to hold an input value, and defaults to `unsigned` which is the output of `std::random_device`.

### Constructors

```c++
entropy_converter();
entropy_converter(entropy_converter&&);
entropy_converter(const entropy_converter&) = delete;
```
Initializes the entropy converter. The copy constructor is deleted as it is not correct to clone the internal entropy.

### = operator

```c++
entropy_converter& operator=(entropy_converter&&);
entropy_converter& operator=(const entropy_converter&) = delete;
```
Moves the internal entropy buffer from another `entropy_converter`. The copy operator is deleted as it is almost certainly not intended, and entropy must never be copied.

### `convert` method

```c++
template<typename Generator>
result_type convert(result_type target, Generator & gen);

template<typename Result, typename Generator>
Result convert(Result outMin, Result outMax, Generator & gen);

template<typename Result, typename Input, typename Generator>
Result convert(Result outMin, Result outMax, Input inMin, Input inMax, Generator & gen,
               result_type limit = std::numeric_limits<result_type>::max());
```
Performs arbitrary entropy conversion. This method reads uniform integers from `gen` and returns a uniform integer in the specified range. 

The *output range* is between `0` and `target-1`, or between `outMin` and `outMax` inclusive. The input range is defined implicitly by `gen.min()` and `gen.max()`, or can be explicitly supplied as arguments `inMin` and `inMax`.

`gen` is a functor that returns a uniform random integer in the input range. It is compatible with C++ random number engines such as `std::random_device` or `std::mt19937`.

`limit` controls the size of buffered entropy, but there is normally no need to specify this as it is generally desirable to buffer as much entropy as possible.

If the input range is a power of 2, then the input range must be represented by `buffer_type`, and the output range must be no more than `limit/2`. If the input range is not a power of 2, then the product of the input and output ranges must not exceed `limit`.

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

`entropy_converter` is not synchronised and concurrent access to an `entropy_converter` is undefined.

## Theoretical background

Producing uniform random numbers from a hardware source presents two challenges:

1) Ensuring that the resulting number is perfectly uniform,
2) Avoiding losing too much entropy in the conversion process.

The simple solution, `rand()%n`, is wrong because the resulting distribution will always be biased, except when n is itself a power of 2. In fact any algorithm using a finite quantity of binary entropy produces a biased distribution, and the only correct solution is to allow for an unbounded amount of input entropy.

A correct solution would be

```
int convert(int output_size, int input_size):
    do:
        x = read_entropy_from_device(input_size)
    while x >= output_size
    return x
```
Whilst this is correct, it is not very efficient in terms of its entropy conversion. It loses entropy for three reasons. Firstly, the algorithm may loop several times, and entropy is lost on each iteration. Secondly, the condition `>=` itself destroys entropy, and thirdly, the residual entropy in `x` is thrown away each iteration. Another limitation is that `input_size` must be larger than `output_size`.

_econv_ uses a modified algorithm that addresses these problems:

```
init():
    value=0
    range=1

int convert(int output_size, int input_size=2):
    do:
        while range < limit / input_size:
            value = value * input_size + read_entropy_from device(input_size)
            range = range * input_size
        new_range = range - range % output_size
        if value < new_range:
            result = value % output_size
            value = value / output_size
            range = new_range / output_size
            return result
        else:
            value -= new_range
            range -= new_range
    loop
```
The algorithm stores entropy in the variable `value`, which is a uniform random integer between `0` and `range-1`. Initially, `value` contains no entropy, but as soon as `convert` is called, the algorithm reads as much entropy as possible from the input source into the `value`, up to a maximum of `limit`. In general, `value` will contain entropy from the previous iteration, with the invariant that `value` is a uniform random variable between `0` and `range-1`.

Next, the algorithm find the highest multiple of `output_size` smaller than `range`, by subtracting the modulus and storing the result in `new_range`. If `value < new_range`, then we know that `value` is a uniform random variable less than `new_range`, so we can factorize `new_range` into `output_size` and the new `range`. We return a random variable of size `output_size`, and the remaining entropy is stored in `value` for the next time `convert()` is called.

If `value < new_range` is false, then value lies between `new_range` and `range-1`. Thus we subtract `new_range` from `value` and `range`, preserving our invariant that `value` is between `0` and `range-1`.

## Analysis
The only place this algorithm loses entropy is in the comparison `value < new_range`. After we have performed `value < new_range`, we either have a random number between `0` and `new_range-1`, or a random number between `new_range` and `range-1`. Both of these random numbers contain less entropy than the original number, hence the entropy has been "lost", or more correctly, transferred into the expression `value < new_range`.

The amount of entropy lost by this comparison can be shown to be the [binary entropy function](https://en.wikipedia.org/wiki/Binary_entropy_function).

Entropy loss per comparison =

```
(1)  -plg(p) - qlg(q)
```

where

```
(2) p = P(value<new_range)
      = new_range / range
      = (range - range%output_size) / range
      > 1 - output_size/range
      > 1 - input_size*output_size/limit       // Since range > limit/input_size
      
    q =  1 - p
      < input_size * output_size / limit
```

The expected number of times we go round the loop is `1/p`, so the expected entropy loss of `convert` is

```
(3)  (-plg(p) - qlg(q))/p
```

We now see the purpose of fetching as much entropy as possible up front. In order to achieve efficient conversion, we make `q` as small as possible, which is done by making `limit` as large as possible (e.g. 2^64-1), and `input_size` as small as possible, i.e. 2. Note that chunks of binary entropy are buffered and read 1 bit at a time, so the `input_size` is taken to be 2.

`limit` is the largest value represented by `result_type`. Therefore `entropy_converter<uint64_t>` is more efficient than `entropy_converter<uint32_t>`, at the expense of slightly more buffering.

## A better estimate of entropy loss

The previous section gave a conservative estimate for entropy loss based on the range of possible values for `range` and `new_range`.

```
    limit/input_size < range <= limit
    range-output_size < new_range <= range
```

If we instead take `range` and `new_range` to be in the middle of their ranges, we get

```
(4) E(p) = E(new_range/range)
         = E(new_range)/E(range)
         = (K-output_size)/(K+1)

       K = limit + limit/input_size
```

We can then use Equation 3 to give us the expected entropy loss. Again we note that larger `limit`, smaller `input_size` and smaller `output_size` give lower entropy loss.

## Results

The following table summarises some of the entropy losses when performing conversion, either per conversion, or per shuffle. Estimated loss is given by `p` from Equation 4, and maximum loss is given by using `p` from Equation 2.

| Conversion to    | From base | Buffer size (bits) | Est. loss (bits) | Max loss (bits) |
|------------------|----------:|-------------------:|-----------------:|----------------:|
| Roll a 1-6       | 2         | 16                 | 0.0011           | 0.0025          |
|                  |           | 32                 | 3.4e-8           | 8.3e-8          |
|                  |           | 64                 | 1.7e-17          | 4.0e-17         | 
| Shuffle 52 cards | 2         | 16                 | 0.19             | 0.48            |
|                  |           | 32                 | 6.4e-6           | 1.8e-5          |
|                  |           | 64                 | 3.3e-15          | 8.87e-15        |
| 1-9              | 10        | 16                 | 0.0020           | 0.015           |
|                  |           | 32                 | 6.4e-8           | 5.6e-7          |
|                  |           | 64                 | 2.4e-17          | 2.9e-16         |
| 1-11             | 10        | 16                 | 0.0023           | 0.018           |
|                  |           | 32                 | 7.6e-8           | 6.8e-7          |
|                  |           | 64                 | 3.7e-17          | 3.5e-16         |

Tests validate that the measured entropy loss does not exceed the maximum loss over a long run. In the case of 32- and 64-bit limits, we appear to systematically overestimate the entropy loss, because events where `value <  new_range` are rare (P~=1e-18), and it is these events that consume the most entropy.
