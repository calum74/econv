// Converts entropy between uniform random integers.
// A typical use is reading entropy from a hardware device (std::random_device),
// and using it to produce uniform integers in a smaller range, or to shuffle
// an array.
//
// This algorithm is exceptionally "efficient" in that its conversion ratio
// of entropy is very nearly 1. This is for situations where we are relying
// on external hardware generator for entropy, and we want to make sure
// that we throw as little of that entropy away as possible.
//
// Example:
//
// entropy_converter<> c;
// std::random_device d;
// std::cout << "You rolled a " << c.convert(1,6,d) << std::endl;

#pragma once

#include <limits>
#include <stdexcept>

// The entopy converter.
// It converts entropy, and buffers a limited amount of entropy.
//
// T is the type used to store output entropy, and 
// Buffer is the type used to buffer the input (base-2) entropy.
//
// Buffer should be at least as large as the data type of the generator,
// usually "unsigned" for std::random_device.
// T should ideally be much larger than the size of the distribution,
// to give a better conversion efficiency.
//
// The 'convert' functions read entropy from one uniform distribution
// and convert it into a uniform distribution of a different size.
template<typename T=unsigned, typename Buffer=unsigned>
class entropy_converter
{
public:
	typedef Buffer buffer_type;
	typedef T result_type;

	// Initialize the converter with zero entropy
	entropy_converter() : value(0), range(1), buffer(0), buffer_max(0)
	{
	}

	// We must not clone the internal entropy.
	// Attempting this is probably a mistake so fail noisily.
	entropy_converter(const entropy_converter&) = delete;
	entropy_converter operator=(const entropy_converter&) = delete;

	// Move the entropy from 'a'.
	entropy_converter(entropy_converter&&a) : value(a.value), range(a.range), buffer(a.buffer), buffer_max(a.buffer_max)
	{
		a.reset();
	}

	// Discard all stored entropy.
	void reset()
	{
		value = 0;
		range = 1;
		buffer = 0;
		buffer_max = 0;
	}

	// Move the entropy from 'a'.
	entropy_converter & operator=(entropy_converter&&a)
	{
		value = a.value;
		range = a.range;
		buffer = a.buffer;
		buffer_max = a.buffer_max;
		a.reset();
		return *this;
	}

	// Reads entropy from gen and returns a uniform random integer.
	// gen is a functor that returns a number in the range [gen.min(),gen.max()]
	// Generator is a uniform random number generator like std::random_device.
	// Return value is in the range [0,target)
	template<typename Generator>
	result_type convert(result_type target, Generator & gen)
	{
		if (target <= 0)
			throw std::range_error("Output range is invalid");
		return convert<result_type>(0, target - 1, gen);
	}

	// Reads entropy from gen and returns a uniform random integer.
	// gen is a functor that returns a number in the range [gen.min(),gen.max()]
	// Generator is a uniform random number generator like std::random_device.
	// Return value is in the range [outMin, outMax]
	template<typename Result, typename Generator>
	Result convert(Result outMin, Result outMax, Generator & gen)
	{
		return convert(outMin, outMax, gen.min(), gen.max(), gen);
	}

	// Reads entropy from gen and returns a uniform random integer.
	// gen is a functor that returns a number in the range [inMin,inMax]
	// limit supplies an optional limit to the amount of buffered entropy.
	// Normally, the algorithm buffers as much entropy as possible.
	// Generator is a uniform random number generator like std::random_device.
	// Return value is in the range [outMin, outMax]
	template<typename Result, typename Input, typename Generator>
	Result convert(Result outMin, Result outMax, Input inMin, Input inMax, Generator & gen, result_type limit = std::numeric_limits<result_type>::max())
	{
		if (outMin == outMax) return outMax;
		if (outMin > outMax)
			throw std::range_error("Invalid output range");
		if (inMin >= inMax)
			throw std::range_error("Invalid input range");

		auto target = 1 + outMax - outMin;
		auto inRange = inMax - inMin;
		if ((inRange & (inRange + 1)) == 0)
		{
			// The generator produces powers of 2. In this case, we
			// buffer the output of gen in 'buffer'.

			if (inRange > (Input)std::numeric_limits<buffer_type>::max())
				throw std::range_error("buffer_size too small");

			return outMin + (Result)convert_from_source(target, 2, limit, [=,&gen]()
			{
				if (buffer_max == 0)
				{
					auto g = gen();
					if (g < inMin)
						throw std::range_error("Input value too small");
					if (g > inMax)
						throw std::range_error("Input value too large");
					buffer = (buffer_type)(g - inMin);
					buffer_max = (buffer_type)inRange;
				}
				auto r = buffer & 1;
				buffer >>= 1;
				buffer_max >>= 1;
				return r;
			});
		}
		else
		{
			if (inRange >= std::numeric_limits<result_type>::max())
				throw std::range_error("buffer_size too small");

			return outMin + (Result)convert_from_source(target, (result_type)(inRange + 1), limit, [=,&gen]()
			{
				return gen() - inMin;
			});
		}
	}

	// Return a functor generating that generates
	template<typename Generator>
	auto with_generator(Generator &gen)
	{
		return [&](result_type target) { return convert(target, gen); };
	}

	// Return a functor generating a uniform random distribution in the range [a,b]
	template<typename Result>
	auto make_uniform(Result a, Result b)
	{
		return [=](auto &gen) { return this->convert(a, b, gen); };
	}

	// Return a functor generating a uniform random distribution in the range [a,b]
	template<typename Result, typename Generator>
	auto make_uniform(Result a, Result b, Generator &gen)
	{
		return [=,&gen]() { return this->convert(a, b, gen); };
	}

	// Returns the size of the buffered entropy.
	// std::log2 of this gives the entropy expressed in bits.
	long double get_buffered_range() const
	{
		return (long double)range * (long double)buffer_max + (long double)range;
	}

private:
	// Reads entropy from source and returns a uniform random number in the range [0,target)
	// source is a functor that returns an integer in the range [0,src_range)
	// limit specifies the maximum size of the entropy to buffer.
	template<typename Source>
	result_type convert_from_source(result_type target, result_type src_range, result_type limit, Source source)
	{
		if (target > limit / src_range)
			throw std::range_error("The output range is too large");

		for (;;)
		{
			// Read as much entropy as possible up-front.
			// This is counterintuitive but gives a very high conversion efficiency.
			while (range < limit / src_range)
			{
				result_type s = (result_type)source();
				if (s < 0) throw
					std::range_error("Input is too small");
				if (s >= src_range) throw
					std::range_error("Input is too large");
				value = value * src_range + s;
				range *= src_range;
			}

			// "new_range" is the highest multiple of target <= range
			result_type new_range = range - range % target;

			if (value < new_range)
			{
				// Transfer the right amount of entropy from "value"
				// into "result", and leave the remaining entropy in "value"
				// for the next time.
				result_type r = value % target;
				value /= target;
				range = new_range / target;
				return r;
			}
			else
			{
				// Recycle the remaining entropy and try again.
				value -= new_range;
				range -= new_range;
			}
		}
	}

	// Stores entropy.
	// "value" is a uniform random integer in [0,range)
	// "buffer" is a uniform random integer in [0,buffer_max].
	// "buffer_max" is a power of 2 - 1
	result_type value, range;
	buffer_type buffer, buffer_max;
};
