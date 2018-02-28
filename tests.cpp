// Various tests and samples for entropy_converter.

#include "entropy_converter.hpp"
#include <random>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <algorithm>

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
	LD p = LD(out - 2)*LD(in) / (3.0*LD(std::numeric_limits<T>::max())), q = 1.0 - p;
	return (-p * std::log2(p) - q * std::log2(q)) / q;
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
	int n = 1000;
	int deck = 52;
	entropy_converter<T> g;
	MeasuringRandomDevice d;
	std::cout << "\nMeasuring " << n << " shuffles of a deck of size " <<
		deck << " using " << sizeof(T)*8 << " bits:\n";

	// Measure actual entropy used generating n shuffles
	LD outputEntropy = 0.0;
	LD maxEntropyLoss = 0.0;
	for (int i = 0; i < n; ++i)
	{
		for (T t = 2; t <= 52; ++t)
		{
			g.convert(t, d);
			outputEntropy += std::log2(LD(t));
			maxEntropyLoss += max_entropy_loss(t);
		}
	}

	LD inputEntropy = d.entropy() - buffered_entropy(g);

	std::cout << std::setprecision(20);
	std::cout << "  Input entropy  = " << inputEntropy << " bits\n";
	std::cout << "  Output entropy = " << outputEntropy << " bits\n";
	std::cout << std::setprecision(6);
	std::cout << "  Measured entropy loss per shuffle = " << (inputEntropy - outputEntropy) / n << " bits\n";
	std::cout << "  Upper bound entropy loss per shuffle = " << maxEntropyLoss / n << " bits\n";
}

// Measure actual entropy used to convert numbers from one base to another.
template<typename T>
void measure_conversion(int from, int to)
{
	entropy_converter<T> c1, c2;
	std::random_device d;

	int n = 1000;
	double inputEntropy = 0.0, outputEntropy = 0.0;
	auto src = [&]() { inputEntropy += std::log2(from); return c1.convert(from, d);  };

	for (int i = 0; i < n; ++i)
	{
		c2.convert(1,to, 0,from-1, src);
		outputEntropy += std::log2(to);
	}

	inputEntropy -= buffered_entropy(c2);

	std::cout << "\nMeasuring " << n << " conversions from " << from << " to " << to << " using " << sizeof(T)*8 << " bits:\n";
	std::cout << std::setprecision(12);
	std::cout << "  Input entropy  = " << inputEntropy << " bits\n";
	std::cout << "  Output entropy = " << outputEntropy << " bits\n";
	std::cout << std::setprecision(6);
	std::cout << "  Measured entropy loss per conversion = " << (inputEntropy - outputEntropy) / n << " bits\n";
	std::cout << "  Upper bound entropy loss per conversion = " << max_entropy_loss(to, from) << " bits\n";
}

// Measure how much entropy is lost in a random sequence of conversions.
template<typename T>
void measure_expected_entropy()
{
	MeasuringRandomDevice d;
	entropy_converter<T> c;
	int n = 1000;
	T t = 50, max = 1000, min = 5;
	long double expectedLoss = 0, maxLoss = 0;
	long double outputEntropy = 0;
	for (int i = 0; i < n; ++i)
	{
		outputEntropy += std::log2(t);
		expectedLoss += expected_entropy_loss(t);
		maxLoss += max_entropy_loss(t);
		auto r = c.convert(t, d);
		t = 2 + r * 2;
		if (t > max) t = max;
		if (t < min) t = min;
	}

	auto inputEntropy = d.entropy() - buffered_entropy(c);
	std::cout << std::setprecision(40) << std::fixed;
	std::cout << "\nMeasuring randomized entropy loss using " << sizeof(T) * 8 << " bits:\n";
	std::cout << "  Input entropy         = " << inputEntropy << " bits\n";
	std::cout << "  Output entropy        = " << outputEntropy << " bits\n";
	std::cout << "  Measured entropy loss = " << (inputEntropy - outputEntropy) << " bits\n";
	std::cout << "  Expected entropy loss = " << expectedLoss << " bits\n";
	std::cout << "  Upper bound loss      = " << maxLoss << " bits\n";
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

	// Shuffle an array
	for (int i = 0; i < 4; ++i)
	{
		const int n = 20;
		int array[n];
		for (int i = 0; i < n; ++i) array[i] = i;

		std::random_shuffle(array, array + n, c.with_generator(d));

		for (int i = 0; i < n; ++i) std::cout << array[i] << " ";
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
			totals[c.convert(range, d)]++;
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
		double inputEntropy = d.entropy() - std::log2(c.get_buffered_range());
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
	auto c16a(c16);
	assert(c16.get_buffered_range() > 1);
	assert(c16a.get_buffered_range() == 1);
	auto c16b(std::move(c16));
	assert(c16.get_buffered_range() == 1);
	assert(c16b.get_buffered_range() > 1);

	// Assignment
	c16a = c16b;
	assert(c16b.get_buffered_range() > 1);
	assert(c16a.get_buffered_range() == 1);
	c16a = std::move(c16b);
	assert(c16a.get_buffered_range() > 1);
	assert(c16b.get_buffered_range() == 1);

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

void measurements()
{
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
		std::cout << "Entropy used by std::uniform_int_distribution(1,6) = " << d.entropy() / n << " bits\n";
	}

	std::cout << "Upper bound entropy loss of generating a 1-6, 64-bit buffer = " << max_entropy_loss(uint64_t(6)) << " bits\n";
	std::cout << "Upper bound entropy loss of shuffling 52 cards, 64-bit buffer = " << max_shuffle_loss(uint64_t(52)) << " bits\n";
	std::cout << "Upper bound entropy loss of generating a 1-6, 32-bit buffer = " << max_entropy_loss(uint32_t(6)) << " bits\n";
	std::cout << "Upper bound entropy loss of shuffling 52 cards, 32-bit buffer = " << max_shuffle_loss(uint32_t(52)) << " bits\n";

	measure_shuffle<std::uint16_t>();
	measure_shuffle<std::uint32_t>();
	measure_shuffle<std::uint64_t>();

	measure_conversion<std::uint32_t>(10, 11);
	measure_conversion<std::uint64_t>(10, 11);
	measure_conversion<std::uint32_t>(10, 9);
	measure_conversion<std::uint64_t>(10, 9);

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
