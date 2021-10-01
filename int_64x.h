#pragma once

#include <iostream>
#include <vector>
#include <string>

//The int_64 class is my attempt at making an integer type of arbitrary length. This
//is accomplished by stringing together unsigned long long types stored in a vector
//which handles the deconstruction. By stringing together multiple 64-bit numbers
//it's possible to make numbers that are 128-bit, 256-bit, etc. up to any arbitrary
//size. This class utilizes built in functions to manipulate these large numbers as
//much as possible and then employs custom algorithms to handle certain things that the
//built in arithmetic functions can't, like calculating the carry between additions. The
//goal of this class was to, first and foremost, learn about overloading operators for a
//custom class, but also to make a type for C++ that can handle large numbers ala Python.
//I've come across numerous libraries online with this goal, however, most of the one's
//that I've found either use a vector of integers to represent long numbers or a string
//to represent long numbers. The vector of integers method can be quite space intensive
//(a 500 digit number for example would need 32-bits * 500 digits = 2kB of storage) while
//the string method takes a very long time to carry out calculations as each character
//needs to be converted to an integer before it can be manipulated. This class will save
//on both space and time by keeping large numbers in a binary representation and splitting
//the number up between multiple 64-bit words (for example the number 9,223,372,036,854,775,808
//would be represented as [0x0000000000000001], [0x0000000000000000]).

//TODO:
//1. The division operator needs to be optimized (haven't attempted this yet so there should be some low hanging fruit here).
//2. >>= operator needs to have the same logic as the <<= operator
//3. Need to add boolean AND, NOT and XOR operators
//4. String initialization and printing need to be optimized
//5. Add versions of each operator that work directly on unsigned longs, should be quicker than converting to int_64x

//Arithmetic Operator Time Comparisons with a Similar Class
//These are time comparisons with another C++ big integer type library that I had initially used from online somewhere.
//The int_64x class probably isn't the fastest big integer class out there but it's nice to have a comparison. I tested
//each operator for speed using either the 5 digit numbers 12345 and 54321, or by using the 100 digit numbers
//1234567890123... and 9876543210987...
//----------------
//+= operator: ~37x faster for 5 digit numbers,  ~14x faster for 100 digit numbers
//+  operator: ~ 2x faster for 5 digit numbers,  ~ 4x faster for 100 digit numbers
//-= operator: ~56x faster for 5 digit numbers,  ~20x faster for 100 digit numbers
//-  operator: ~ 4x faster for 5 digit numbers,  ~ 3x faster for 100 digit numbers
//*= operator: ~49x faster for 5 digit numbers, ~177x faster for 100 digit numbers
//*  operator: ~ 2x faster for 5 digit numbers,  ~27x faster for 100 digit numbers (something is screwy here for the 5 digit case)
///= operator: ~.5x faster for 5 digit numbers,  ~.3x faster for 100 digit numbers
// / operator: ~ 6x faster for 5 digit numbers,  ~12x faster for 100 digit numbers
//----------------

class int_64x
{
public:
	//VARIABLES

	//Digits holds the binary equivalent of our number. In order to acheive an arbitrary length a vector is utilized.
	//The digits are in Big Endian format, however, the 64-bit word order is reversed. So a 192 bit word would be ordered like so (where
	//0 is the LSB and 191 is the MSB):
	//[63, 62, 61, ... 0], [127, 126, 125 ... 64], [191, 190, 189, ... 128]
	std::vector<unsigned long long> digits;

	//CONSTRUCTORS
	int_64x() = default; //no special default operator
	int_64x(int number); //create a new int_64x type from a signed integer
	int_64x(unsigned int number); //create a new int_64x type from an unsigned integer
	int_64x(long long number); //create a new int_64x type from a signed long long integer
	int_64x(unsigned long long number, int left_shift = 0); //create a new int_64x type by left shifting the given unsigned long long
	int_64x(std::string number); //create a new int_64x type via a string of numbers, the best way for instantiating a long variable
	int_64x(const int_64x& num); //Copy constructor

	//ARITHMETIC OPERATORS
	//Addition Operators
	int_64x& operator+=(const int_64x& num);
	friend int_64x operator+(const int_64x& num1, const int_64x& num2);

	//Subtraction Operators
	int_64x& operator-=(const int_64x& num);
	friend int_64x operator-(const int_64x& num1, const int_64x& num2);

	//Multiplication Operators
	int_64x& operator*=(const int_64x& num);
	friend int_64x operator*(const int_64x& num1, const int_64x& num2); //TODO: This operator is going really slow with small numbers but not big numbers, investigate and find out why
	void FastMultiplication(const int_64x& num);

	//Division Operators
	int_64x& operator/=(const int_64x& num); //TODO: This operator hasn't yet been optimized, come back at some point and spend some time on this
	friend int_64x operator/(const int_64x& num1, int_64x& num2);

	//BINARY OPERATORS
	//Left Shift Operators
	int_64x& operator<<=(const unsigned int left_shift);
	friend int_64x operator<<(const int_64x& num, const unsigned int left_shift);

	//Right Shift Operators
	int_64x& operator>>=(const unsigned int left_shift); //TODO: Update so this has same logic as <<= operator
	friend int_64x operator>>(const int_64x& num, const unsigned int right_shift);

	//OR Operators
	int_64x& operator |= (const int_64x& num);
	friend int_64x operator|(const int_64x& num1, const int_64x& num2);

	//TODO: add AND operators

	//BOOLEAN OPERATORS
	friend bool operator==(const int_64x& num1, const int_64x& num2);
	friend bool operator!=(const int_64x& num1, const int_64x& num2);
	friend bool operator<(const int_64x& num1, const int_64x& num2);
	friend bool operator>(const int_64x& num1, const int_64x& num2);
	friend bool operator<=(const int_64x& num1, const int_64x& num2);
	friend bool operator>=(const int_64x& num1, const int_64x& num2);

	//Assignment Operators
	int_64x& operator=(const int_64x& num);
	
	//OTHER FUNCTIONS
	std::string getNumberString();
	void partialAddition(unsigned long long num, int word); //make this private after testing
	void zero();
};

//Related Functions
bool CompareArraySize(int* one, int* two, int elements);
void MultiplyArrayByTwo(int* numbers, int elements);
void DivideArrayByTwo(int* numbers, int elements);
void AddArrays(int* one, int* two, int elements);
void SubtractArrays(int* one, int* two, int elements);
void twosComplement(int_64x& num);
int fastlog2(unsigned long long value);
int GetLeadBitLocation(int_64x &num);
void unsignedAddition(unsigned long long* num1, unsigned long long num2, int num1_size, int word);

//Printing Functions
std::ostream& operator<<(std::ostream& os, int_64x& num);
void PrintBinary(int_64x num);
