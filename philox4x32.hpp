// 乱数生成器．n 番目までシークする時間が O(1) なので都合がよい．
// ref: https://en.cppreference.com/w/cpp/numeric/random/philox_engine

#pragma once

#include <cstdint>
#include <array>

namespace my_rng
{
	template<std::size_t round, std::uint_fast32_t... consts>
	struct philox32_base {
		using result_type = std::uint_fast32_t;
		constexpr static std::size_t word_count = sizeof... (consts);
		constexpr static std::size_t word_size = 32;
		constexpr static std::size_t round_count = round;
		static_assert((word_count == 2 || word_count == 4) && (round_count > 0));

	private:
		template<size_t d, size_t r>
		static consteval std::array<result_type, word_count / d> skipped_array() {
			std::array<result_type, word_count / d> ret{};
			std::array<result_type, word_count> src { consts... };
			for (size_t k = 0; k < word_count / d; k++)
				ret[k] = src[d * k + r];
			return ret;
		}

	public:
		constexpr static std::array<result_type, word_count / 2> multipliers = skipped_array<2, 0>();
		constexpr static std::array<result_type, word_count / 2> round_consts = skipped_array<2, 1>();
		constexpr static std::uint_least32_t default_seed = 20111115u;

		static constexpr result_type min() { return 0u; }
		static constexpr result_type max() { return ~0u; }

		// constructors.
		constexpr philox32_base() : philox32_base(default_seed) {}
		constexpr explicit philox32_base(result_type value)
		{
			seed(value);
		}
		template<class seed_seq>
		requires (!std::convertible_to<seed_seq, result_type> && !std::same_as<seed_seq, philox32_base>)
		constexpr explicit philox32_base(seed_seq& seq)
		{
			seed(seq);
		}

		// seedings.
		constexpr void seed(result_type value)
		{
			for (auto& x : X) x = 0;
			K[0] = value; for (std::size_t k = 1; k < std::size(K); k++) K[k] = 0;
			j = word_count - 1;
		}
		template<class seed_seq>
			requires (!std::convertible_to<seed_seq, result_type>)
		constexpr void seed(seed_seq& seq)
		{
			// reset Z.
			for (auto& x : X) x = 0;

			// handle seq.
			result_type a[word_count / 2];
			seq.generate(a + 0, a + word_count / 2);
			for (std::size_t k = 0; k < word_count / 2; k++)
				K[k] = a[k];

			// reset j.
			j = word_count - 1;
		}
		constexpr void set_couner(std::array<result_type, word_count> const& c)
		{
			for (std::size_t k = 0; k < word_count; k++)
				X[k] = c[word_count - 1 - k];
			j = word_count - 1;
		}

		// PRNG operations.
		constexpr result_type operator()()
		{
			move_next();
			return Y[j];
		}
		constexpr void discard(uint64_t z)
		{
			if (z < word_count - j) {
				j += static_cast<std::size_t>(z);
				return;
			}

			z += j;
			j = z % word_count;
			z /= word_count;

			auto x01 = (static_cast<std::uint_fast64_t>(X[1]) << word_size) | X[0];
			x01 += z;
			X[0] = static_cast<result_type>(x01);
			X[1] = static_cast<result_type>(x01 >> word_size);
			if constexpr (word_count == 4) {
				if (x01 < z) {
					for (std::size_t k = 2; k < word_count && ++X[k] == 0; ++k);
				}
			}

			if (j != word_count - 1) update();
		}

		// identify.
		constexpr bool operator==(philox32_base const& rhs) const
		{
			return j == rhs.j
				&& X == rhs.X
				&& K == rhs.K;
		}

	private:
		// implementations of PRNG.
		std::size_t j = 0;
		std::array<result_type, word_count> X; // represents a big integer Z = \sum_k 2^{w k} X_k.
		std::array<result_type, word_count> Y; // generated sequence.
		std::array<result_type, word_count / 2> K; // "key" sequence (essentially a seed).

		constexpr void update_round(std::size_t q)
		{
			if constexpr (word_count == 4) {
				auto x01 = static_cast<std::uint_fast64_t>(Y[2]) * multipliers[0];
				auto x23 = static_cast<std::uint_fast64_t>(Y[0]) * multipliers[1];
				Y[0] = static_cast<result_type>(x01 >> word_size)
					^ static_cast<result_type>(K[0] + q * round_consts[0]) ^ Y[1];
				Y[1] = static_cast<result_type>(x01);
				Y[2] = static_cast<result_type>(x23 >> word_size)
					^ static_cast<result_type>(K[1] + q * round_consts[1]) ^ Y[3];
				Y[3] = static_cast<result_type>(x23);
			}
			else {
				auto x01 = static_cast<std::uint_fast64_t>(Y[0]) * multipliers[0];
				Y[0] = static_cast<result_type>(x01 >> word_size)
					^ static_cast<result_type>(K[0] + q * round_consts[0]) ^ Y[1];
				Y[1] = static_cast<result_type>(x01);
			}
		}
		constexpr void update()
		{
			Y = X;
			for (std::size_t q = 0; q < round_count; q++) update_round(q);
		}
		constexpr void move_next()
		{
			if (j != word_count - 1) j++;
			else {
				// generate Y.
				update();

				// increment Z.
				for (size_t k = 0; k < word_count && ++X[k] == 0; ++k);

				// reset j.
				j = 0;
			}
		}
	};

	using philox4x32 = philox32_base<10,
		0xCD9E8D57, 0x9E3779B9,
		0xD2511F53, 0xBB67AE85>;
}
