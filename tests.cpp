// Various tests and samples for entropy_converter.

#include "entropy_converter.hpp"
#include <random>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <numeric>

typedef long double LD;

// The expected maximum entropy loss from a conversion.
// Note that the entropy can exceed this but not on average.
template<typename T>
LD max_entropy_loss(T out, T in = 2)
{
	LD p = LD(out)*LD(in) / LD(std::numeric_limits<T>::max()), q = 1.0 - p;
	return (-p * std::log2(p) - q * std::log2(q)) / q;
}

// A slightly tighter bound on expected entropy loss.
// Assumes random targets which isn't quite true.
template<typename T>
LD expected_entropy_loss(T out, T in = 2)
{
	LD limit = std::numeric_limits<T>::max();
	LD k = limit + limit/in;
	LD p = (k+2-out)/(k+1);
	LD q = (out-1)/(k+1); // = 1-p
	return (-p * std::log2(p) - q * std::log2(q)) / p;
}

// Loss if we never fail need to iterate
template<typename T>
LD best_entropy_loss(T out, T in = 2)
{
	LD limit = std::numeric_limits<T>::max();
	LD k = limit + limit / in;
	return std::log2(LD(k + 1)) - std::log2(LD(k + 2 - out));  // -lg(p)
}


template<typename T>
LD min_efficiency(T out)
{
	return std::log2(out) / (max_entropy_loss(out) + std::log2(out));
}

template<typename T>
LD max_shuffle_loss(T n)
{
	LD loss = 0.0;
	for (T i = 2; i <= n; ++i)
		loss += max_entropy_loss(i);
	return loss;
}

// How much entropy is required to shuffle a deck of size n?
LD shuffle_output_entropy(int n)
{
	LD e = 0.0;
	for (int i = 2; i <= n; ++i)
		e += std::log2(i);
	return e;
}

// What's the worst-case efficiency shuffling a deck of cards?
// This is quite a loose bound.
template<typename T>
LD shuffle_efficiency(T n)
{
	auto se = shuffle_output_entropy(n);
	return se / (se + max_shuffle_loss(n));
}

// How much entropy is stored inside the entropy converter?
// Use this mainly when measuring how efficient we are.
template<typename T>
LD buffered_entropy(const entropy_converter<T> & c)
{
	return std::log2(c.get_buffered_range());
}

// A random device where we keep track of the quantity of entropy
// it has produced.
class MeasuringRandomDevice
{
public:
	typedef unsigned result_type;

	MeasuringRandomDevice() : count(0) { }
	result_type operator()() { ++count; return d(); }
	LD entropy() const { return LD(count*sizeof(result_type)*8); }
	static std::size_t min() { return std::random_device::min(); }
	static std::size_t max() { return std::random_device::max(); }
private:
	std::random_device d;
	unsigned count;
};

// Measures actual entropy consumed by a shuffle.
template<typename T>
void measure_shuffle()
{
	int n = 10000;
	int deck = 52;
	entropy_converter<T> g;
	MeasuringRandomDevice d;

	// Measure actual entropy used generating n shuffles
	LD outputEntropy = 0.0;
	LD maxEntropyLoss = 0.0;
	LD expectedEntropyLoss = 0.0;
	LD bestEntropyLoss = 0.0;
	for (int i = 0; i < n; ++i)
	{
		for (T t = 2; t <= deck; ++t)
		{
			g.convert(t, d);
			outputEntropy += std::log2(LD(t));
			maxEntropyLoss += max_entropy_loss(t);
			expectedEntropyLoss += expected_entropy_loss(t);
			bestEntropyLoss += best_entropy_loss(t);
		}
	}

	LD inputEntropy = d.entropy() - buffered_entropy(g);

	std::cout << std::setprecision(6);
	std::cout
		<< "| Shuffle " << deck
		<< " | " << sizeof(T) * 8 
		<< " | " << bestEntropyLoss / n
		<< " | " << expectedEntropyLoss / n
		<< " | " << maxEntropyLoss / n
		<< " | " << n
		<< " | " << (inputEntropy - outputEntropy)/n
		<< " | " << std::setprecision(15) << inputEntropy
		<< " | " << outputEntropy
		<< " |\n";
}

// Measure actual entropy used to convert numbers from one base to another.
template<typename T>
void measure_conversion(T from, T to)
{
	entropy_converter<T> c1, c2;
	std::random_device d;

	int n = 10000;
	LD inputEntropy = 0.0, outputEntropy = 0.0;
	auto src = [&]() { inputEntropy += std::log2(from); return c1.convert(from, d);  };

	for (int i = 0; i < n; ++i)
	{
		c2.convert(T(1), to, T(0), T(from-1), src);
		outputEntropy += std::log2(to);
	}

	inputEntropy -= buffered_entropy(c2);
	auto loss = (inputEntropy - outputEntropy) / n;
	if (loss < 0) loss = NAN;

	std::cout << std::setprecision(6);
	std::cout
		<< "| Convert " << from << " to " << to
		<< " | " << sizeof(T) * 8
		<< " | " << best_entropy_loss(to,from)
		<< " | " << expected_entropy_loss(to, from)
		<< " | " << max_entropy_loss(to, from)
		<< " | " << n 
		<< " | " <<  loss
		<< " | " << std::setprecision(15) << inputEntropy
		<< " | " << outputEntropy
		<< " |\n";
}

// Measure how much entropy is lost in a random sequence of conversions.
template<typename T>
void measure_expected_entropy()
{
	MeasuringRandomDevice d;
	entropy_converter<T> c;
	int n = 10000;
	T t = 50, max = 1000, min = 5;
	LD expectedLoss = 0, maxLoss = 0, bestLoss=0;
	LD outputEntropy = 0;
	for (int i = 0; i < n; ++i)
	{
		outputEntropy += std::log2(t);
		expectedLoss += expected_entropy_loss(t);
		maxLoss += max_entropy_loss(t);
		bestLoss += best_entropy_loss(t);
		auto r = c.convert(t, d);
		t = 2 + r * 2;
		if (t > max) t = max;
		if (t < min) t = min;
	}

	auto inputEntropy = d.entropy() - buffered_entropy(c);
	auto totalLoss = inputEntropy - outputEntropy;
	if (totalLoss < 0) totalLoss = NAN;
	std::cout << std::setprecision(6);
	std::cout 
		<< "| Randomized sequence | " << sizeof(T) * 8
		<< " | " << bestLoss
		<< " | " << expectedLoss
		<< " | " << maxLoss
		<< " | " << n
		<< " | " << totalLoss
		<< " | " << std::setprecision(15) << inputEntropy
		<< " | " << outputEntropy
		<< " |\n";
}

void examples()
{
	// Create a converter
	entropy_converter<> c;

	// Use a hardware random number generator
	std::random_device d;

	// Create a distribution
	auto d6 = c.make_uniform(1, 6);

	// Roll a die
	std::cout << "You rolled a " << d6(d) << std::endl;

	// Shuffle a deck of cards
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
}

// Ensure that a distribution is somewhat uniform.
// Test whether numbers are generated in correct proportion.
template<typename T>
void test_distribution_is_uniform(T range)
{
	entropy_converter<T> c;
	std::random_device d;
	long double outputEntropy = 0.0;
	std::vector<std::size_t> totals(range);
	unsigned count = 0;
	bool valid;
	do
	{
		int n = 1000;
		for (int i = 0; i < n; ++i)
		{
			auto x = c.convert(range, d);
			assert(x >= 0 && x < range);
			totals[x]++;
			count++;
		}
		outputEntropy += n * std::log2((long double)range);
		valid = true;
		auto expectedCount = count / range;
		for (T i = 0; i < range; ++i)
		{
			if (totals[i] < expectedCount * 9 / 10 || totals[i] > expectedCount * 11 / 10)
			{
				valid = false;
				break;
			}
		}
	} while (!valid);
}

// Check that entropy_converter uses the expected amount of entropy.
template<typename T>
void test_entropy_consumption(T target)
{
	double loss = 0, expected = 0.01;
	const int n = 1000;
	do
	{
		MeasuringRandomDevice d;
		entropy_converter<T> c;
		for (int i = 0; i < n; ++i)
			c.convert(target, d);
		loss += d.entropy() - std::log2(c.get_buffered_range()) - n * std::log2(target);
		expected += n * max_entropy_loss(target);
	} while (loss > expected);
}

template<typename Fn>
void assert_throws(Fn fn)
{
	try
	{
		fn();
		assert(!"Expected exception not thrown");
	}
	catch (std::range_error)
	{
	}
}

void tests()
{
	std::cout << "\nRunning tests\n";
	std::random_device d;

	// Constructors
	entropy_converter<std::uint16_t> c16;
	entropy_converter<std::uint32_t> c32;
	entropy_converter<std::uint64_t> c64;

	// Initial range = 1
	assert(c16.get_buffered_range() == 1);

	c16.convert(2, d);
	assert(c16.get_buffered_range() > 1);

	// Copying attempts
	assert(c16.get_buffered_range() > 1);
	auto c16b(std::move(c16));
	assert(c16.get_buffered_range() == 1);
	assert(c16b.get_buffered_range() > 1);

	// Assignment
	assert(c16b.get_buffered_range() > 1);
	c16 = std::move(c16b);
	assert(c16b.get_buffered_range() == 1);

	// Test range checks
	assert_throws([&]() {c16.convert(-1,d);});
	assert_throws([&]() {c16.convert(0x8000, d);});
	c16.convert(0x1000000, 0x10001000, d); // Range ok.
	assert_throws([&]() { c16.convert(10, 5, d); });
	auto gen1 = []() { return 1; };
	assert_throws([&]() { c16.convert(1, 0x4000, 1, 0x4000, gen1); });
	assert_throws([&]() { c16.convert(1, 100, 2, 10, gen1); });
	assert_throws([&]() { c16.convert(1, 100, 2, 3, gen1); });
	assert_throws([&]() { c16.convert(1, 100, 1, 1, gen1); });
	assert_throws([&]() { c16.convert(1, 100, 2, 1, gen1); });

	// Test the quality of the output

	for (int i = 1; i < 100; ++i)
	{
		test_distribution_is_uniform<std::uint16_t>(i);
		test_distribution_is_uniform<std::uint32_t>(i);
		test_distribution_is_uniform<std::uint64_t>(i);
		test_entropy_consumption<std::uint16_t>(i);
		test_entropy_consumption<std::uint32_t>(i);
		test_entropy_consumption<std::uint64_t>(i);
	}

	std::cout << "Tests passed\n";
}

void measure_conversions(int from, int to)
{
	measure_conversion<std::uint16_t>(from, to);
	measure_conversion<std::uint32_t>(from, to);
	measure_conversion<std::uint64_t>(from, to);
}

void measurements()
{
#ifndef __clang__  // Not working on clang due to bug in library
	{
		int array[52];
		MeasuringRandomDevice d;
		std::shuffle(array, array + 52, d);
		std::cout << "\nEntropy used by std::shuffle = " << d.entropy() << " bits\n";
	}

	{
		MeasuringRandomDevice d;
		std::uniform_int_distribution<> d6(1, 6);
		int n = 1000;
		for (int i = 0; i < n; ++i)
			d6(d);
		std::cout << "Entropy used by std::uniform_int_distribution(1,6) = " << d.entropy() / n << " bits\n\n";
	}
#endif

	std::cout << "| Test | Buffer size (bits) | Best loss (bits) | Estimated loss (bits) | Max loss (bits) | Iterations | Measured loss (bits) | Input entropy (bits) | Output entropy (bits) |\n";
	std::cout << "|------|-------------------:|-----------------:|----------------------:|----------------:|-----------:|---------------------:|---------------------:|----------------------:|\n";
	measure_shuffle<std::uint16_t>();
	measure_shuffle<std::uint32_t>();
	measure_shuffle<std::uint64_t>();

	measure_conversions(2,6);
	measure_conversions(10,9);
	measure_conversions(10,11);

	measure_expected_entropy<std::uint16_t>();
	measure_expected_entropy<std::uint32_t>();
	measure_expected_entropy<std::uint64_t>();
}

int main()
{
	examples();
	measurements();
	tests();
}
