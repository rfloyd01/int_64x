#pragma once

#include <iostream>
#include <vector>
#include <string>

//The int_64x class is my attempt at making an integer type of arbitrary length. This
//is accomplished by stringing together multiple (although not necessarily consecutive)
//unsigned long long int types together to make a single cohesive number. Instead of
//adding an entire int_64x to another int_64x type, the individual unsigned long longs
//that make up each number are manipulated individually and then are brought back 
//together to form a new int_64x type

//This class will be able to handle numbers of any size as long as it fits in the user's
//memory.

//Time Comparisons
//----------------

class int_64x
{
public:
	int_64x();  //default constructor
	~int_64x(); //default destructor

	int_64x(int number); //create a new int_64x type from a signed integer
	int_64x(unsigned int number); //create a new int_64x type from an unsigned integer
	int_64x(long long number); //create a new int_64x type from a signed long long integer
	int_64x(unsigned long long number); //create a new int_64x type from an unsigned long long integer
	int_64x(unsigned long long number, int left_shift); //create a new int_64x type by left shifting the given unsigned long long
	int_64x(std::string number); //create a new int_64x type via a string of numbers, the best way for instantiating a long variable
	int_64x(int zero, int number_longs); //create a new int_64x type that has "number_longs" long longs all initialized to 0
	int_64x(const int_64x& num); //Copy constructor

	//std::vector<unsigned long long*> getDigits();

	//Arithmetic Operators
	int_64x& operator+=(const int_64x& num);
	int_64x& operator+=(const unsigned long long& num); //TODO: this should be updated to match new += operator
	int_64x& operator-=(const int_64x& num);
	int_64x& operator-=(const unsigned long long& num); //TODO: this should be updated to match new -= operator
	//int_64x& operator*=(int_64x& num); //TODO: Do what's necessary to change "num" to a const (and try to apply Katsuba)
	int_64x& operator*=(const int_64x& num);
	int_64x& operator*=(int num);
	int_64x& operator/=(int_64x& num); //TODO: Do what's necessary to change "num" to a const
	//int_64x& operator/=(const unsigned long long& num); //TODO: Come back to this at some point
	int_64x operator/(int_64x& num);
	void FastMultiplication(const int_64x& num); //this was supposed to be a much quicker alternative to normal addition, but it's only a few % quicker

	//Binary Operators
	//TODO: I'm not complete satisfied with the <<= operater, see if it can be reduced at all. Maybe only focus on positive numbers
	int_64x& operator<<=(const unsigned int left_shift);
	int_64x& operator>>=(const unsigned int left_shift); //TODO: Update so this has same logic as new <<= operator
	int_64x& operator |= (int_64x& num);

	int_64x& operator=(const int_64x& num);
	
	std::string getNumberString();

	//Digits holds the binary equivalent of our number. In order to acheive an arbitrary length a vector is utilized.
	//The digits are in Big Endian format, however, the 64-bit word order is reversed. So a 192 bit word would be ordered like so (where
	//0 is the LSB and 191 is the MSB):
	//[63, 62, 61, ... 0], [127, 126, 125 ... 64], [191, 190, 189, ... 128]
	std::vector<unsigned long long*> digits;
	void partialAddition(unsigned long long num, int word); //make this private after testing
private:
	void zero();
};

//Helper Functions and Operators
bool CompareArraySize(int* one, int* two, int elements);
void MultiplyArrayByTwo(int* numbers, int elements);
void DivideArrayByTwo(int* numbers, int elements);
void AddArrays(int* one, int* two, int elements);
void SubtractArrays(int* one, int* two, int elements);
//void PrintBinary(int_64x& num);
void PrintBinary(int_64x num);
void twosComplement(int_64x& num);
int_64x flipBits(int_64x& num);
int fastlog2(unsigned long long value);
int GetLeadBitLocation(int_64x &num);
int_64x Karatsuba(const std::vector<unsigned long long> num1, const std::vector<unsigned long long> num2, unsigned long long num1_polarity, unsigned long long num2_polarity);
void unsignedAddition(unsigned long long* num1, unsigned long long num2, int num1_size, int word);

//Can print int_64x types by reference or copy
std::ostream& operator<<(std::ostream& os, int_64x& num);

//Addition operators allow for addition of int_64x types with unsigned or signed long long and ints
//as well as other int_64x types with implicit conversion
//TODO: It would be great if all of these could be combined into a single function, look int this at some point
//int_64x operator+(int_64x& num1, int_64x& num2);

//Friendly Arithmetic Operators
int_64x operator+(const int_64x& num1, const int_64x& num2);
int_64x operator-(int_64x num1, int_64x& num2);
int_64x operator-(int_64x num1, int& num2);
int_64x operator*(int_64x& num1, int_64x& num2);
int_64x operator*(int_64x num1, int num2);

//Friendly Binary Operators
int_64x operator<<(int_64x num, const unsigned int left_shift);
int_64x operator>>(int_64x& num, const unsigned int right_shift);
int_64x operator|(int_64x num1, int_64x& num2);

//Friendly Comparison Operators
bool operator==(const int_64x& num1, const int_64x& num2);
bool operator!=(const int_64x& num1, const int_64x& num2);
bool operator<(const int_64x& num1, const int_64x& num2);
bool operator>(const int_64x& num1, const int_64x& num2);
bool operator<=(const int_64x& num1, const int_64x& num2);
bool operator>=(const int_64x& num1, const int_64x& num2);

//NOTES
//Currently to create and destroy an instance from a built-int types takes 9.31e-8 seconds, creating 1,000,000 takes 0.093 seconds
//1,000,000 normal built in types can be created in 3e-7 seconds. It probably is so much longer for the int_64x's because there's a
//vector which needs to be created and deleted. Vectors are 24 bytes where's the maximum built-in types is only 8 bytes so I'd expect
//it to take ~4 times longer to create and destroy the int_64x type based on number of things getting created. The 0.093 seconds feels
//like it's longer than it should be currently. Maybe in the future I wont use a vector and just link all of the 64-bit words of the
//int_64x type with pointers in a linked list fashion.

//Currently to create and destroy an instance from a string takes 1.61e-6 seconds, creating 1,000,000 takes 1.61 seconds. This is
//obviously much slower than creating from one of the built in types but that's to be expected as there's a lot more entailed in
//doing it this way. This method is really only intended for creating instances for numbers that are already really large which isn't
//possible with a built in type. I can probably make it more efficient to create int_64x types in this manner, however, the object
//is less about creating large numbers from the get-go and more about building up to large numbers through arithmetic.

//I'll need to find a more efficient way of creating and destroying instances of this class at some point
