#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <chrono>

#ifdef __riscv_vector
#include <riscv_vector.h>
#endif



template <typename T, typename...>
struct IsUInt64
	: std::conditional<
	std::is_integral<T>::value&& std::is_unsigned<T>::value && (sizeof(T) == sizeof(std::uint64_t)),
	std::true_type, std::false_type>::type
{
};

template <typename T, typename U, typename... Rest>
struct IsUInt64<T, U, Rest...>
	: std::conditional<IsUInt64<T>::value&& IsUInt64<U, Rest...>::value, std::true_type, std::false_type>::type
{
};

template <typename T, typename... Rest>
constexpr bool is_uint64_v = IsUInt64<T, Rest...>::value;


static const unsigned long deBruijnTable64[64] = { 63, 0,  58, 1,  59, 47, 53, 2,  60, 39, 48, 27, 54, 33, 42, 3,
												   61, 51, 37, 40, 49, 18, 28, 20, 55, 30, 34, 11, 43, 14, 22, 4,
												   62, 57, 46, 52, 38, 26, 32, 41, 50, 36, 17, 19, 29, 10, 13, 21,
												   56, 45, 25, 31, 35, 16, 9,  12, 44, 24, 15, 8,  23, 7,  6,  5 };

using namespace std;
using namespace chrono;


int bits_per_uint64 = 64;

int arr_bits(uint64_t arr) {
	int bit_count = 0;

	if (arr != 0)
	{

		unsigned long r = 0;
		unsigned long* result = &r;
		// Spread the highest bit downwards
		arr |= arr >> 1;
		arr |= arr >> 2;
		arr |= arr >> 4;
		arr |= arr >> 8;
		arr |= arr >> 16;
		arr |= arr >> 32;

		;

		*result = deBruijnTable64[((arr - (arr >> 1)) * uint64_t(0x07EDD5E59A4E28C2)) >> 58];

		bit_count = static_cast<int>(r + 1);
	}

	return bit_count;
}

int ptr_bits(uint64_t* ptr)
{
	size_t c = 2; // Assuming 128-bit number (2 x 64-bit words)
	ptr += c - 1; // Move pointer to the most significant 64-bit word

	if (*ptr == 0 && c > 1)
	{
		c--; // Reduce count if MSB part is 0
		ptr--; // Move to the next 64-bit part
	}

	int bit_count = 0;
	uint64_t V = *ptr; // Load the most significant part

	if (V == 0)
	{
		bit_count = static_cast<int>(c - 1) * 64;
	}
	else
	{
		unsigned long r = 0;
		unsigned long* result = &r;
		unsigned long* result_bits = result;

		// Set all bits lower than the highest set bit
		V |= V >> 1;
		V |= V >> 2;
		V |= V >> 4;
		V |= V >> 8;
		V |= V >> 16;
		V |= V >> 32;

		*result_bits = deBruijnTable64[((V - (V >> 1)) * uint64_t(0x07EDD5E59A4E28C2)) >> 58];

		bit_count = static_cast<int>(c - 1) * 64 + static_cast<int>(r + 1);
	}

	return bit_count;
}



void divide_uint128_uint64_inplace_generic(uint64_t num[123467], uint64_t denom[123467], uint64_t quot[123467]);
uint64_t correct(uint64_t* numerator, uint64_t* quotient, int numerator_bits, int denominator_shift);
void left_shift_mine(const std::uint64_t* operand, int shift_amount, std::uint64_t* result);
void right_shift_mine(const std::uint64_t* operand, int shift_amount, std::uint64_t* result);




template <typename T, typename S, typename R, typename = std::enable_if_t<is_uint64_v<T, S, R>>>
inline unsigned char sub_uint64_mine(T operand1, S operand2, R* result)
{
	*result = operand1 - operand2;
	return static_cast<unsigned char>(operand2 > operand1);
}

template <typename T, typename S, typename = std::enable_if_t<is_uint64_v<T, S>>>
inline unsigned char sub_uint64_generic_mine(T operand1, S operand2, unsigned char borrow, unsigned long long* result)
{
	auto diff = operand1 - operand2;
	*result = diff - (borrow != 0);
	return (diff > operand1) || (diff < borrow);
}

inline unsigned char sub_mine(const std::uint64_t* operand1, const std::uint64_t* operand2, std::size_t uint64_count, std::uint64_t* result)
{

	// Unroll first iteration of loop. We assume uint64_count > 0.
	unsigned char borrow = sub_uint64_mine(*operand1++, *operand2++, result++);

	// Do the rest
	for (; --uint64_count; operand1++, operand2++, result++)
	{
		unsigned long long temp_result;
		borrow = sub_uint64_generic_mine(*operand1, *operand2, borrow, &temp_result);
		*result = temp_result;
	}
	return borrow;
}




template <typename T, typename S, typename R, typename = std::enable_if_t<is_uint64_v<T, S, R>>>
inline unsigned char add_uint64_mine(T operand1, S operand2, R* result)
{
	*result = operand1 + operand2;
	return static_cast<unsigned char>(*result < operand1);
}

template <typename T, typename S, typename = std::enable_if_t<is_uint64_v<T, S>>>
inline unsigned char add_uint64_generic_mine(T operand1, S operand2, unsigned char carry, unsigned long long* result)
{
	operand1 += operand2;
	*result = operand1 + carry;
	return (operand1 < operand2) || (~operand1 < carry);
}

inline unsigned char add_uint_mine(const std::uint64_t* operand1, const std::uint64_t* operand2, std::size_t uint64_count, std::uint64_t* result)
{

	// Unroll first iteration of loop. We assume uint64_count > 0.
	unsigned char carry = add_uint64_mine(*operand1++, *operand2++, result++);

	// Do the rest
	for (; --uint64_count; operand1++, operand2++, result++)
	{
		unsigned long long temp_result;
		carry = add_uint64_generic_mine(*operand1, *operand2, carry, &temp_result);
		*result = temp_result;
	}
	return carry;
}





uint64_t correct(uint64_t* numerator, uint64_t* quotient, int numerator_bits, int denominator_shift) {
	if (numerator_bits > 0)
	{
		right_shift_mine(numerator, denominator_shift, numerator);
	}
	return quotient[0];
}

void left_shift_mine(const std::uint64_t* operand, int shift_amount, std::uint64_t* result) {

	const std::size_t bits_per_uint64_sz = static_cast<std::size_t>(bits_per_uint64);
	const std::size_t shift_amount_sz = static_cast<std::size_t>(shift_amount);
  std::size_t bit_shift_amount = shift_amount_sz & (bits_per_uint64_sz - 1);

  
  #if defined (__riscv_v_intrinsic)
    size_t vl = 2;
    vuint64m1_t vec;

    if (shift_amount_sz & bits_per_uint64) {
        // Word shift needed: [0, operand[0]]
        uint64_t op0 = operand[0];
        vec = __riscv_vmv_v_x_u64m1(0, vl);         // Initialize to all zeros
        vec = __riscv_vslide1down_vx_u64m1(vec, op0, vl); // Set element 1 to op0
    } else {
        // No word shift: load original values
        vec = __riscv_vle64_v_u64m1(operand, vl); // Load 2 elements
    }

    // Handle bit-level shift
    if (bit_shift_amount) {
        // Shift left and get overflow bits
        vuint64m1_t shifted_left = __riscv_vsll_vx_u64m1(vec, bit_shift_amount, vl);
        vuint64m1_t shifted_right = __riscv_vsrl_vx_u64m1(vec, 64 - bit_shift_amount, vl);
        
        // Adjust shifted_right to move overflow bits to upper elements
        vuint64m1_t shifted_right_adj = __riscv_vslide1up_vx_u64m1(shifted_right, 0, vl);
        
        // Combine results
        vec = __riscv_vor_vv_u64m1(shifted_left, shifted_right_adj, vl);
    }

    // Store results
    __riscv_vse64_v_u64m1(result, vec, vl);
   #else 

	  // Early return
  	if (shift_amount_sz & bits_per_uint64_sz)
  	{
  		result[1] = operand[0];
  		result[0] = 0;
  	}
  	else
  	{
  		result[1] = operand[1];
  		result[0] = operand[0];
  	}
  
  	// How many bits to shift in addition to word shift
  	
  
  	// Do we have a word shift
  	if (bit_shift_amount)
  	{
  		std::size_t neg_bit_shift_amount = bits_per_uint64_sz - bit_shift_amount;
  		// Warning: if bit_shift_amount == 0 this is incorrect
  		result[1] = (result[1] << bit_shift_amount) | (result[0] >> neg_bit_shift_amount);
  		result[0] = result[0] << bit_shift_amount;
	  }
   #endif
}

void right_shift_mine(const std::uint64_t* operand, int shift_amount, std::uint64_t* result) {

	const std::size_t bits_per_uint64_sz = static_cast<std::size_t>(bits_per_uint64);
	const std::size_t shift_amount_sz = static_cast<std::size_t>(shift_amount);
  std::size_t bit_shift_amount = shift_amount_sz & (bits_per_uint64_sz - 1);

  #if defined(__riscv_v_intrinsic)
    size_t vl = 2; // Process 2 elements (128 bits)
    vuint64m1_t vec;

    // Handle word-level shift (64-bit multiples)
    if (shift_amount_sz & bits_per_uint64) {
        // Word shift needed: [operand[1], 0]
        uint64_t op1 = operand[1];
        vec = __riscv_vmv_v_x_u64m1(0, vl); // Initialize to all zeros
        vec = __riscv_vslide1up_vx_u64m1(vec, op1, vl); // Set element 0 to op1
    } else {
        // No word shift: load original values
        vec = __riscv_vle64_v_u64m1(operand, vl); // Load 2 elements
    }

    // Handle bit-level shift
    if (bit_shift_amount) {
        // Shift right and get overflow bits
        vuint64m1_t shifted_right = __riscv_vsrl_vx_u64m1(vec, bit_shift_amount, vl);
        vuint64m1_t shifted_left = __riscv_vsll_vx_u64m1(vec, bits_per_uint64 - bit_shift_amount, vl);

        // Adjust shifted_left to move overflow bits to lower elements
        vuint64m1_t shifted_left_adj = __riscv_vslide1down_vx_u64m1(shifted_left, 0, vl);

        // Combine results
        vec = __riscv_vor_vv_u64m1(shifted_right, shifted_left_adj, vl);
    }

    // Store results
    __riscv_vse64_v_u64m1(result, vec, vl);
   #else 
  	if (shift_amount_sz & bits_per_uint64_sz)
  	{
  		result[0] = operand[1];
  		result[1] = 0;
  	}
  	else
  	{
  		result[1] = operand[1];
  		result[0] = operand[0];
  	}
  
  	// How many bits to shift in addition to word shift
  
  	if (bit_shift_amount)
  	{
  		std::size_t neg_bit_shift_amount = bits_per_uint64_sz - bit_shift_amount;
  
  		// Warning: if bit_shift_amount == 0 this is incorrect
  		result[0] = (result[0] >> bit_shift_amount) | (result[1] << neg_bit_shift_amount);
  		result[1] = result[1] >> bit_shift_amount;
  	}
   #endif
}

void divide_uint128_uint64_inplace_generic(uint64_t num[123467], uint64_t denom[123467], uint64_t quot[123467])
{
	constexpr size_t uint64_count = 2;

	for (int i = 0; i < 123467; i++) {


		uint64_t _numerator[2];
		uint64_t* numerator = &_numerator[0];
		numerator[0] = 0;
		numerator[1] = num[i];

		uint64_t denominator = denom[i];


		uint64_t _quotient[2];
		uint64_t* quotient = &_quotient[0];
		// Clear quotient
		quotient[0] = 0;
		quotient[1] = 0;

		// Get significant bits
		int numerator_bits = ptr_bits(numerator);  // number of ms of num 

		int denominator_bits = arr_bits(denominator);  //num of ms of denom


		if (numerator_bits < denominator_bits)
		{
			return;
		}

		// Create storage for shifted denominator
		uint64_t shifted_denominator[uint64_count]{ denominator, 0 };
		uint64_t difference[uint64_count]{ 0, 0 };

		// Shift denominator
		int denominator_shift = numerator_bits - denominator_bits;  // number of shifts of denom


		left_shift_mine(shifted_denominator, denominator_shift, shifted_denominator);
		


		denominator_bits += denominator_shift;

		int remaining_shifts = denominator_shift;




		while (numerator_bits == denominator_bits)
		{

			if (sub_mine(numerator, shifted_denominator, uint64_count, difference))
			{
				if (remaining_shifts == 0)
				{
					break;
				}

				add_uint_mine(difference, numerator, uint64_count, difference);

				quotient[1] = (quotient[1] << 1) | (quotient[0] >> (63));
				quotient[0] <<= 1;
				remaining_shifts--;
			}

			numerator_bits = ptr_bits(difference);

			int numerator_shift = std::min(denominator_bits - numerator_bits, remaining_shifts);

			numerator[0] = 0;
			numerator[1] = 0;

			if (numerator_bits > 0)
			{
				left_shift_mine(difference, numerator_shift, numerator);

				numerator_bits += numerator_shift;
			}

			quotient[0] |= 1;

			left_shift_mine(quotient, numerator_shift, quotient);
			

			remaining_shifts -= numerator_shift;
		}

		
		quot[i] = correct(numerator, quotient, numerator_bits, denominator_shift);
		
	}

}





int main() {

	uint64_t *num = (uint64_t*)malloc(sizeof(uint64_t) * 123467);
	ifstream f1;
	f1.open("/home/greghas/Myfunc/numerator.txt");
	int i = 0;
	if (f1.is_open()) {
		while (!f1.eof())
		{
			f1>>num[i];
			i++;

		}
	}
	f1.close();


	uint64_t *denom = (uint64_t*)malloc(sizeof(uint64_t) * 123467);
	ifstream f2;
	f2.open("/home/greghas/Myfunc/denom.txt");
	i = 0;
	if (f2.is_open()) {
		while (!f2.eof())
		{
			f2>>denom[i];
			i++;
		}
	}
	f2.close();



	uint64_t *quot = (uint64_t*)malloc(sizeof(uint64_t) * 123467);


	auto start = high_resolution_clock::now();

	divide_uint128_uint64_inplace_generic(num, denom, quot);

	auto stop = high_resolution_clock::now();
	auto duration = duration_cast<microseconds>(stop - start);
	cout <<"Execution time:" <<  duration.count() << "ms" << endl;

	free(num);
	free(denom);
	uint64_t *quot_check = (uint64_t*)malloc(sizeof(uint64_t) * 123467);

	ifstream f3;
	f3.open("/home/greghas/Myfunc/qu.txt");
	i = 0;
	if (f3.is_open()) {
		while (!f3.eof())
		{
			f3>>quot_check[i];
			i++;
		}
	}
	f3.close();


	int w = 0;
	for (int i = 0; i < 123467; i++) {
		if (quot_check[i] != quot[i]) {
			std::cout << "WRONG!!!" << std::endl;
			w = 1;
			break;
		}
	}

	if (!w) {
		std::cout << "CORRECT!!!" << std::endl;
	}

	free(quot);
	free(quot_check);
	return 0;
}