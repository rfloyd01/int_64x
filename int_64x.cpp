#include <Header_Files/pch.h>
#include <Header_Files/print.h>
#include <Header_Files/int_64x.h>
#include <iostream>
#include <bitset>
#include <cmath>
#include <stdexcept>

//Table for calculation of log2(x)
const int tab64[64] = {
	63,  0, 58,  1, 59, 47, 53,  2,
	60, 39, 48, 27, 54, 33, 42,  3,
	61, 51, 37, 40, 49, 18, 28, 20,
	55, 30, 34, 11, 43, 14, 22,  4,
	62, 57, 46, 52, 38, 26, 32, 41,
	50, 36, 17, 19, 29, 10, 13, 21,
	56, 45, 25, 31, 35, 16,  9, 12,
	44, 24, 15,  8, 23,  7,  6,  5 };

//Destructors
int_64x::~int_64x()
{
	//std::cout << "Destructor called on instance " << std::endl;
	//need to free up all of the memory set aside for the int_64x
	int len = digits.size();
	for (int i = 0; i < len; i++)
	{
		delete digits[i];
	}
	digits.clear();
}

//Constructors
int_64x::int_64x()
{
	//std::cout << "Default Constructor called on instance " << this << std::endl;
	//default initialize to zero
	unsigned long long* init = new unsigned long long;
	*init = 0;
	digits.push_back(init);
}
int_64x::int_64x(int number)
{
	//std::cout << "Int Constructor called on instance " << this << std::endl;
	//it doesn't matter whether a positive or a negative integer is passed, either can be converted directly
	//to an unsigned long long and then stored on the heap
	unsigned long long* init = new unsigned long long;
	*init = (unsigned long long) number;
	digits.push_back(init);
}
int_64x::int_64x(unsigned int number)
{
	//std::cout << "Unsigned Int Constructor called on instance " << this << std::endl;
	//an unsigned int can just be cast directly to an unsigned long long
	unsigned long long* init = new unsigned long long;
	*init = (unsigned long long) number;
	digits.push_back(init);
}
int_64x::int_64x(long long number)
{
	//a standard long long can be positive or negative, we can just initialize the int_64x type as is
	unsigned long long* init = new unsigned long long;
	*init = (unsigned long long) number;
	digits.push_back(init);
}
int_64x::int_64x(unsigned long long number)
{
	//an unsigned long long can only be positive, therefore if the lead bit is a 1 then we need to add
	//a word of all 0's to the front of the int_64x type so that it retains its positivity.
	unsigned long long* init = new unsigned long long;
	*init = number;
	digits.push_back(init);
	if (number >> 63)
	{
		unsigned long long* zero = new unsigned long long;
		*zero = 0;
		digits.push_back(zero);
	}
}
int_64x::int_64x(unsigned long long number, int left_shift)
{
	//This constructor is useful during the multiplication process. Takes "number" and creates a new
	//int_64x type by shifting it left by "left_shift" number of times. We need to be careful about
	//whether or not "number" is positive or negative. Positive numbers will get 0's added to as the MSB
	//in case of overflow while negative numbers will get 1's.

	//if leftshift is zero then just create instance from number
	if (left_shift == 0)
	{
		unsigned long long* init = new unsigned long long;
		*init = number;
		digits.push_back(init);
		return;
	}

	bool negative = false;
	unsigned long long adder = 0;
	if (number & 0x8000000000000000) negative = true;
	
	//If left shifting by a factor of 64 then "number" will stay intact
	if (left_shift % 64 == 0)
	{
		for (int i = 0; i < (left_shift / 64); i++)
		{
			unsigned long long* init = new unsigned long long;
			*init = adder;
			digits.push_back(init);
		}
		unsigned long long* init = new unsigned long long;
		*init = number;
		digits.push_back(init);
	}
	else
	{
		//if left shifting by something that isn't 0 mod 64 then "number" is going to get split up
		//between 64-bit words
		unsigned long long* MSB = new unsigned long long;
		unsigned long long* LSB = new unsigned long long;

		*MSB = (number >> (64 - ((left_shift % 64))));
		*LSB = (number << (left_shift % 64));

		if (negative)
		{
			*MSB |= (0xFFFFFFFFFFFFFFFF << (left_shift % 64));
			//*LSB |= (0xFFFFFFFFFFFFFFFF >> (64 - ((left_shift % 64))));
		}

		for (int i = 0; i < (left_shift / 64); i++)
		{
			unsigned long long* init = new unsigned long long;
			*init = adder;
			digits.push_back(init);
		}
		digits.push_back(LSB);
		digits.push_back(MSB);
	}

	//Sometimes left shifting a positive number will create an unnecessary empty 64-bit word as the MSB,
	//discard the MSBs if this is the case
	if (!negative && (*digits.back() == 0))
	{
		if (*digits[digits.size() - 2] & 0x8000000000000000) return; //we must keep the leading zeros in this case
		delete digits.back();
		digits.erase(digits.end() - 1); //remove the current MSB from digit lits
	}
}
int_64x::int_64x(std::string number)
{
	//std::cout << "String Constructor called on instance " << this << std::endl;
	//this is the main way to initialize an int_64x type. A string of numbers, positive or negative can be passed
	//and then converted. The only acceptable characters are the digits 0-9 and the '-' symbol, which will only
	//be accepted as the very first character in the string otherwise it's not allowed.

	//First store digits of number in an array. Then create a different array which will hold powers of two.
	//Keep multiplying the two array by two until it's larger than the number held in the other array. From here,
	//keep dividing the power of two array by 2 until it's smaller than the number array and then subtract the two array from
	//the number array. The BigInt_128 will need to be built bit-by-bit in this fashion. For now this is the quickest way
	//I can think of to construct a binary representation of a number from a string of characters.

	//funky things happen if a string of "1" is passed to this function, best to just check for this case and manually set
	//TODO: there's probably a much better way to implement this check, come back to this at some point
	if (number == "1" || number == "-1")
	{
		unsigned long long* init = new unsigned long long;
		*init = 1;
		if (number == "-1") *init *= -1;
		digits.push_back(init);
		return;
	}

	bool negative = false; //if the first character of number is the '-' symbol this will be set to true so we know to flip the bits later on
	int end_point = 0; //this marks where we stop reading the string "number"
	if (number[0] == '-')
	{
		negative = true;
		end_point = 1; //don't want to try and incorporate the '-' sign into our actual number
	}

	//all arrays in this algorithm are backwards, i.e. given a string of "12345" an array of [5, 4, 3, 2, 1] will be created
	int* base_ten  = new int[number.length() + 1]();
	int* two_power = new int[number.length() + 1](); //at most two_power will need to be one digit longer than base_ten
	two_power[0] = 1; //start with 2^0 = 1

	int power = 0; //using a signed int means the largest number we can make is 2^(2^(31 - 1)) = 2^2,147,483,647 = 10^~646,456,993 (aka a long effing number)
	for (int i = number.length(); i > end_point; i--)
	{
		if ((number[i - 1] > 57) || (number[i - 1] < 48))
		{
			//need to make sure that we're only reading digits, otherwise we might get some garbage in the input string
			//TODO: add in some way to skip characters encountered that aren't actual digits. This would cause shifting everything already
			//      in the array and I don't want to do it right now.
			std::cout << "Found something in the number that's not a real digit, skipping it." << std::endl;
			continue;
		}
		base_ten[number.length() - i] = number[i - 1] - 48; //remove 48 to get from character representation of digit to actual number
	}

	//multiply two_power by 2 until it is larger than the number now stored in base_ten
	while (!CompareArraySize(two_power, base_ten, number.length() + 1))
	{
		MultiplyArrayByTwo(two_power, number.length() + 1);
		power++;
	}

	//as soon as two_power becomes larger than base_ten divide it by two and reduce the power count by 1
	DivideArrayByTwo(two_power, number.length() + 1);
	power--;

	//Next we need to see how many unsigned long long integers will be necessary to hold the entire number, this is fairly easy to
	//do by looking at the value of power. If power equals 200 for example then we need cieling(200 / 64) = 4 unsigned long longs
	//to hold all of the binary digits.
	int number_of_longs = power / 64 + 1;

	for (int i = 0; i < number_of_longs; i++)
	{
		unsigned long long* more_digits = new unsigned long long;
		digits.push_back(more_digits);
	}

	//now subtract two_power from base_ten and set the appropriate bit in digits
	unsigned long long num = 0, placement; //num keeps track of the actual number while placement tells us where to place the next binary digit
	while (power > 0)
	{
		placement = (unsigned long long)1 << (power % 64);
		//go to the next 64-bit unsigned long long whenever power = 0 modulo 63
		while (power % 64 != 0)
		{
			if (!CompareArraySize(two_power, base_ten, number.length() + 1)) //two_power is smaller than base_ten
			{
				SubtractArrays(base_ten, two_power, number.length() + 1); //subtract two_power from base_ten
				num |= placement;
			}
			DivideArrayByTwo(two_power, number.length() + 1);
			placement >>= 1;
			power--;
		}

		//once pow = 0 % 64 we've hit the final binary digit of the current 64-bit unsigned long long so we need to move to the next one
		//need to do one final check. TODO - consider putting this check into a separate function to avoid writing it out twice
		if (!CompareArraySize(two_power, base_ten, number.length() + 1)) //two_power is smaller than base_ten
		{
			SubtractArrays(base_ten, two_power, number.length() + 1); //subtract two_power from base_ten
			num |= placement;
		}
		DivideArrayByTwo(two_power, number.length() + 1);
		*digits[number_of_longs - 1] = num;
		number_of_longs--; //TODO: I think i can convert the above line to *digits[--number_of_longs] = num and have the same effect
		num = 0;
		power--;
	}

	//We've succesfully turned the string into a binary representation, make sure to free up the memory that we no longer need
	delete[] base_ten;
	delete[] two_power;

	//once we've set all the bits, we need to flip them and then add 1 (two's complement) if the number was defined as a negative,
	//otherwise we're done
	if (negative) twosComplement(*this);
}
int_64x::int_64x(int zero, int number_longs)
{
	//std::cout << "Variable Zero Constructor called on instance " << this << std::endl;
	//this constructor will create a new instance where the value is zero but the number is 64 * number_longs bits long
	//If number_longs is 0 then the int_64x will be initialized with no words in it, which is currently the only way to
	//do this
	for (int i = 0; i < number_longs; i++)
	{
		unsigned long long* init = new unsigned long long;
		*init = 0;
		digits.push_back(init);
	}
}
int_64x::int_64x(const int_64x& num)
{
	//std::cout << this << " Created from a copy of " << &num << std::endl;
	//copy constructor for class, utilizes the overloaded = operator
	*this = num;
}

//ARITHMETIC OPERATORS
//Addition Operators
int_64x& int_64x::operator+=(const int_64x& num)
{
	//C++ can handle addition of unsigned long longs without any problem so we add each word from *this to the same word in num and then manually
	//keep track of whether or not each of these additions results in an overflow. Once all of these partial additions has been complete, we need
	//to determine whether or not the final sum remains the same, has it's most significant word removed or has another word added to the front.

	//The first thing we need to do is compare the polarities of the two numbers, this will let us know if the sum should be larger or smaller than
	//the original number
	unsigned long long different_polarity = *(this->digits.back()) ^ *num.digits.back();
	int diff = num.digits.size() - this->digits.size(), carry = 0; //carry keeps track of overflow between addition of individual 64-bit words

	//if num has more words than *this we need to add empty words so that the lenghts are the same. The below loop will only execute if num
	//has more words than *this
	for (int i = 0; i < diff; i++)
	{
		unsigned long long* new_word = new unsigned long long;
		*new_word = 0;
		this->digits.push_back(new_word);
	}

	unsigned long long this_temp, num_temp; //temporary variables are created on stack to reduce calls to heap memory
	//add all of the 64-bit words from num to the 64-bit words from *this
	for (int i = 0; i < num.digits.size(); i++)
	{
		this_temp = *(this->digits[i]);
		num_temp = *num.digits[i];
		//first add the carry bit if it's non-zero
		if (carry)
		{
			//*(this->digits[i]) += carry;
			this_temp += carry;

			//it's unlikely, but possible that adding the carry bit will overflow the current word, in which case digits[i] will equal 0. If
			//digits[i] == 0 then carry remains at 1, otherwise carry is set to 0.
			//carry = (!(*(this->digits[i])));
			carry = (!this_temp);
		}

		//Then add the two numbers together. If *this.digits[i] ends up being less than num.digits[i] after the addition we know that overflow has occurred.
		/**(this->digits[i]) += *num.digits[i];
		if (*(this->digits[i]) < *num.digits[i]) carry = 1;*/
		this_temp += num_temp;
		if (this_temp < num_temp) carry = 1;
		*(this->digits[i]) = this_temp;
	}

	//in the case that *this has more words than num we need another loop to handle any more potential carry over. This loop will only
	//commence if *this has more words than num. If num is negative we also need to add 0xFFFFFFFFFFFFFFFF during each iteration
	if (*num.digits.back() & 0x8000000000000000)
	{
		for (int i = 0; i < (this->digits.size() - num.digits.size()); i++)
		{
			//Need to add the implied 0xFFFFFFFFFFFFFFFF at the front of the smaller negative number
			/**(this->digits[num.digits.size() + i]) += carry;
			carry = carry && (!((bool)*(this->digits[num.digits.size() + i])));
			*(this->digits[num.digits.size() + i]) += 0xFFFFFFFFFFFFFFFF;
			if (*(this->digits[num.digits.size() + i]) < 0xFFFFFFFFFFFFFFFF) carry = 1;*/
			this_temp = *(this->digits[num.digits.size() + i]);
			this_temp += carry;
			carry = carry && (!this_temp);
			this_temp += 0xFFFFFFFFFFFFFFFF;
			if (this_temp < 0xFFFFFFFFFFFFFFFF) carry = 1;
			*(this->digits[num.digits.size() + i]) = this_temp;
		}
	}
	else
	{
		//No need to add anything extra here as we're adding to words that are 0x0000000000000000, we can break as soon as there's no carry
		if (carry)
		{
			for (int i = 0; i < (this->digits.size() - num.digits.size()); i++)
			{
				*(this->digits[num.digits.size() + i]) += carry;
				carry = carry && (!((bool)*(this->digits[num.digits.size() + i])));
				if (!carry) break;
			}
		}
	}

	//The addition is done at this point, however, we need to either add a new word for numbers that have reversed polarity that shouldn't have,
	//or remove a word from the front of the number that have become redundant (or just leave the answer as is)
	if (different_polarity >> 63)
	{
		//the original numbers had different polarities, we need to either remove a word(s) or keep the answer the same. We only need to remove a
		//redundant word if the most significant word is 0 or 0xFFFFFFFFFFFFFFFF.
		
		//we need to remove the most significant word under two conditions. First, the length of *this is greater than 1
		//and second, the polarity of the most significant word matches that of the second most significant word
		int start = this->digits.size() - 1;
		for (int i = start; i >= 1; i--)
		{
			//this for loop will only execute if the length of *this is greater than 1 word and will terminate before elimating the first word
			if ((*(this->digits[i]) == 0) || (*(this->digits[i]) == 0xFFFFFFFFFFFFFFFF))
			{
				if (!((*(this->digits[i]) ^ *(this->digits[i - 1])) >> 63))
				{
					//polarities of final two words are the same so remove the final word
					delete this->digits[i];
					this->digits.erase(this->digits.end() - 1);
				}
				else break;
			}
		}
		return *this;
	}
	else
	{
		//the original numbers had the same polarity, we need to either add a new word or keep the answer the same
		if ((*(this->digits.back()) ^ *num.digits.back()) >> 63)
		{
			//polarity of the answer has flipped, we need to either add a 0 or 0xFFFFFFFFFFFFFFFF to the front
			unsigned long long* new_word = new unsigned long long;
			*new_word = 0 - (*num.digits.back() >> 63);
			this->digits.push_back(new_word);
		}
		return *this;
	}
	return *this;
}
int_64x& int_64x::operator+=(const unsigned long long& num)
{
	//this operator is useful for quickly adding smaller numbers to an int_64x type without the need for creating a new instance of the int_64 type.
	//The construction / destruction process for the int_64x type is fairly slow so best to avoid it whenever possible

	//The first thing we need to do is keep a note of whether *this is a positve or negative number as this fact will be lost after initial addition
	bool negative = *(this->digits.back()) & 0x8000000000000000;
	int adder = 0;
	if (num < 0) adder = 0xFFFFFFFFFFFFFFFF;
	int num_size = 1, this_size = this->digits.size();
	int carry = 0; //carry keeps track of overflow between addition of individual 64-bit words

	//add "num" to the first word of *this
	*(this->digits[0]) += num;
	if (*(this->digits[0]) < num) carry = 1;

	//cycle through the rest of the words in *this and add any carry as necessary
	for (int i = 0; i < (this_size - num_size); i++)
	{
		//same as the loop above except the value added from num will either be 0 or 0xFFFFFFFFFFFFFFFF
		*(this->digits[num_size + i]) += carry;
		carry = carry && (!((bool)*(this->digits[num_size + i])));
		*(this->digits[num_size + i]) += adder;
		if (*(this->digits[num_size + i]) < adder) carry = 1;
	}

	//Now we need to see if ovflow has happened in the most significant 64 bits. Since the MSB here represents + or - and isn't part of the
	//actual number the overflow is handled a little differently depending on whether the orignal numbers were positve or negative. These cases
	//are handled separately.

	//Case 1: Overflow from Addition of two positive numbers.
	//An overflow will occur if we add two positive numbers together and the MSB get's turned into a 1 (which signifies a negative number). All of the
	//binary digits should actually be correct in this case, we just need to add a new empty unsigned long long to the front of the number to ensure
	//that the lead digit is a zero and our number stays positive
	if ((!negative) & (num >= 0))
	{
		if (*(this->digits.back()) & 0x8000000000000000)
		{
			//overflow has occured, add a new empty 64-bit word to front of number
			unsigned long long* zero = new unsigned long long;
			*zero = 0;
			this->digits.push_back(zero);
		}
		return *this;
	}

	//Case 2: Overflow from Addition of two negative numbers
	//Negative numbers are a little bit trickier here as adding them will always cause an overflow in the traditional sense because the MSB is always a 1.
	//For example, -1 + -1 looks like 1111 + 1111 = 11110. The sum doesn't fit into 4 bits, however, truncating the MSB (as will happen automatically)
	//yields 1110 which is the correct value of -2. So in this case we just need to ignore the overflow to get the correct number. Here's another case. -8 + -8
	//looks like 1000 + 1000 = 10000. 10000 does indeed equal -16, however, if the MSB gets truncated here then we're left with 0000 which is no longer correct. So
	//in one case truncating gave the correct answer and in the other truncating didn't. What we can do so the addition works in both cases is simply to do the same
	//thing (albeit opposite) of what we did when adding two positive numbers. Check to see if the leading bit is a 0, if so add a new 64-bit word of all 1's, otherwise
	//do nothing.
	if (negative & (num < 0))
	{
		if (!(bool)(*(this->digits.back()) & 0x8000000000000000))
		{
			//overflow has occured, add a new empty 64-bit word to front of number
			unsigned long long* zero = new unsigned long long;
			*zero = 0xFFFFFFFFFFFFFFFF;
			this->digits.push_back(zero);
		}
		return *this;
	}

	//Case 3: Overflow from Addition of a positive and a negative number
	//Overflow doesn't happen in the traditional sense here. Technically you couldn't overflow at all in this case because adding a positve and negative together
	//both makes the positve smaller (further from overflowing) and the negative bigger (again further from overflowing). Let's look at a few examples: -3 + 6
	//1101 + 0110 = 10011. Truncating the MSB here leads to 0011 which is 3 so it's correct. -8 + 7 = 1000 + 0111 = 1111 which is -1 and correct. Indeed, no matter
	//what way you look at it, truncating the MSB will always yield the correct answer, so if we've gotten to this point of the algorithm then just return our
	//answer

	//Before returning the answer we need to make sure there aren't any unnecessary word at the front.
	//TODO:: I really don't like the below loops, pretty time consuming when compared to normal addition and just to make sure there aren't extra words in the front.
	//There's got to be a more efficient check afor this. Come back to this at some point
	if (this->digits.size() > 1)
	{
		if ((*(this->digits.back()) == 0) && !(*(this->digits[this->digits.size() - 2]) & 0x8000000000000000))
		{
			//extra word(s) of all 0's at front, delete all of these words
			int delete_amount = 0;
			for (int i = this->digits.size() - 1; i > 0; i--)
			{
				if (*this->digits[i] == 0)
				{
					delete this->digits[i]; //free up memory from last element
					delete_amount++;
				}
				else break;
			}

			this->digits.erase(this->digits.end() - delete_amount, this->digits.end()); //remove last element from vector
		}
		else if ((*(this->digits.back()) == 0xFFFFFFFFFFFFFFFF) && (*(this->digits[this->digits.size() - 2]) & 0x8000000000000000))
		{
			//extra word(s) of all 1's at front, delete all of these words
			int delete_amount = 0;
			for (int i = this->digits.size() - 1; i > 0; i--)
			{
				if (*this->digits[i] == 0xFFFFFFFFFFFFFFFF)
				{
					delete this->digits[i]; //free up memory from last element
					delete_amount++;
				}
				else break;
			}
			this->digits.erase(this->digits.end() - delete_amount, this->digits.end()); //remove last element from vector
		}
	}
	return *this;
}
int_64x operator+(const int_64x& num1, const int_64x& num2)
{
	//define addition using the already defined += operator
	int_64x num1_copy = num1;
	num1_copy += num2;
	return num1_copy;
}

//Subtraction Operators
int_64x& int_64x::operator-=(const int_64x& num)
{
	//Subtraction can be approached in the exact same way as addition, however, just in the opposite manner. Starting from the LSB and
	//moving towards the MSB subtract the current word in num from the current word of *this. If the value is greater than the original
	//value of this then an overflow has occured. Instead of adding 1 to the next word like in addition, we remove 1 from the next word
	//and so on and so forth. Subtraction is inherently slower than addition, but since built in functions are being used they should be
	//on par with each other.

	//Original Subtraction Code, just flipped num and then added to this. It worked fine but was slower than desired
	//twosComplement(num);
	//*this += num;
	//twosComplement(num); //make sure to flip num back to it's original state
	//return *this;

	//The first thing we need to do is compare the polarities of the two numbers, this will let us know if the sum should be larger or smaller than
	//the original number
	unsigned long long different_polarity = *(this->digits.back()) ^ *num.digits.back();
	int diff = num.digits.size() - this->digits.size(), carry = 0; //carry keeps track of overflow between addition of individual 64-bit words

	//if num has more words than *this we need to add empty words so that the lenghts are the same. The below loop will only execute if num
	//has more words than *this. Make sure to push back the appropriate word (0 for positive and -1 for negative).
	for (int i = 0; i < diff; i++)
	{
		unsigned long long* new_word = new unsigned long long;
		*new_word = 0 - (*(this->digits.back()) >> 63);
		this->digits.push_back(new_word);
	}

	unsigned long long temp;
	//subtract all of the 64-bit words in num from the 64-bit words in *this
	for (int i = 0; i < num.digits.size(); i++)
	{
		//first remove the carry bit if it's non-zero
		if (carry)
		{
			//it's unlikely, but possible that removing the carry bit will overflow the current word, in which case digits[i] will equal 0xFFFFFFFFFFFFFFFF.
			//If digits[i] == 0xFFFFFFFFFFFFFFFF then carry remains at 1, otherwise carry is set to 0. Carry must be set before the subtraction for easier
			//formula.
			carry = (!(*(this->digits[i])));
			*(this->digits[i]) -= 1;
		}

		//Then subtract the two numbers. If *this.digits[i] ends up increasing after the subtraction we know that overflow has occurred.
		//In order to compare *this.digits[i] with itself after subtraction a temporary variable is necessary.
		temp = *(this->digits[i]) - *num.digits[i];
		if ( temp > *(this->digits[i])) carry = 1;
		*(this->digits[i]) = temp;
	}

	//In the case that *this has more words than num we need another loop to handle any more potential carry over. This loop will only
	//commence if *this has more words than num. If num is negative we also need to add 0xFFFFFFFFFFFFFFFF during each iteration
	if (*num.digits.back() & 0x8000000000000000)
	{
		for (int i = 0; i < (this->digits.size() - num.digits.size()); i++)
		{
			//Need to add the implied 0xFFFFFFFFFFFFFFFF at the front of the smaller negative number
			if (carry)
			{
				//it's unlikely, but possible that removing the carry bit will overflow the current word, in which case digits[i] will equal 0xFFFFFFFFFFFFFFFF.
				//If digits[i] == 0xFFFFFFFFFFFFFFFF then carry remains at 1, otherwise carry is set to 0. Carry must be set before the subtraction.
				carry = (!(*(this->digits[num.digits.size() + i])));
				*(this->digits[num.digits.size() + i]) -= 1;
			}
			temp = *(this->digits[num.digits.size() + i]) - 0xFFFFFFFFFFFFFFFF; //TODO: Might be able to change this to + 1 for a little speedup
			if (temp > *(this->digits[num.digits.size() + i])) carry = 1;
			*(this->digits[num.digits.size() + i]) = temp;
		}
	}
	else
	{
		//No need for extra subtractions as we'd be subtracting 0x0000000000000000, we can break as soon as there's no carry
		if (carry)
		{
			for (int i = 0; i < (this->digits.size() - num.digits.size()); i++)
			{
				carry = (!(*(this->digits[num.digits.size() + i])));
				*(this->digits[num.digits.size() + i]) -= 1;
				if (!carry) break;
			}
		}
	}

	//The subtraction is done at this point, however, we need to either add a new word for numbers that have reversed polarity that shouldn't have,
	//or remove words from the front of the number that have become redundant (or just leave the answer as is)
	if (different_polarity >> 63)
	{
		//the original numbers had different polarities, we need to either add a new word or keep the answer the same
		if (!((*(this->digits.back()) ^ *num.digits.back()) >> 63))
		{
			//if *this and num now have the same polarity then polarity of the answer has flipped, we need to
			//either add a 0 or 0xFFFFFFFFFFFFFFFF to the front
			unsigned long long* new_word = new unsigned long long;
			*new_word = 0 - !(*num.digits.back() >> 63);
			this->digits.push_back(new_word);
		}
		return *this;
	}
	else
	{
		//the original numbers had the same polarity, we need to either remove a word or keep the answer the same. We only need to remove a
		//redundant word if the most significant word is 0 or 0xFFFFFFFFFFFFFFFF.

		//we need to remove the most significant word under two conditions. First, the length of *this is greater than 1
		//and second, the polarity of the most significant word matches that of the second most significant word
		int start = this->digits.size() - 1;
		for (int i = start; i >= 1; i--)
		{
			//this for loop will only execute if the length of *this is greater than 1 word and will terminate before elimating the first word
			if ((*(this->digits[i]) == 0) || (*(this->digits[i]) == 0xFFFFFFFFFFFFFFFF))
			{
				if (!((*(this->digits[i]) ^ *(this->digits[i - 1])) >> 63))
				{
					//polarities of final two words are the same so remove the final word
					delete this->digits[i];
					this->digits.erase(this->digits.end() - 1);
				}
				else break;
			}
		}
		return *this;
	}
	return *this;
}
int_64x& int_64x::operator-=(const unsigned long long& num)
{
	//TODO: see if it's quicker to create a copy of num and then add inverse, or use actual subtraction algorithm.
	//Flip and addition = 0.360548 seconds for 100,000,000 iterations
	//Subtraction = ???
	//instead of doing an actual subtraction, we just turn num into it's own two's complement and then add it to *this
	unsigned long long new_num = ~(unsigned long long)num + 1;
	*this += new_num;
	return *this;
}
int_64x operator-(int_64x num1, int_64x& num2)
{
	//define subtraction using the already defined -= operator
	num1 -= num2;
	return num1;
}
int_64x operator-(int_64x num1, int& num2)
{
	//define subtraction using the already defined -= operator
	num1 -= num2;
	return num1;
}

//Multiplication Operators
int_64x& int_64x::operator*=(const int_64x& num)
{
	//std::cout << "*= operator called." << std::endl;
	//The goal here is to multiply two int_64x types using the Karatsuba multiplication method. The number of multiplications that
	//needs to be carried out is the number of 64-bit words in *this x 64-bit words in num (so when multiplying two numbers that each
	//have two 64-bit words, a total of 4 Karatsuba multiplications will take place).

	//If the numbers being multiplied are small enough we can do all the arithmetic with memory located on the stack instead of the heap
	//which ends up being ~100x quicker.
	if ((this->digits.size() <= 50) && (num.digits.size() <= 50))
	{
		FastMultiplication(num);
		return *this;
	}

	unsigned long long z2, z0, a, b, c, d; //these variables hold the value of the partial multiplications at each iteration
	long long z1; //z1 has the potential to become negative so make it a signed long long

	//first we need to see if *this or num are negative, if so, flip them and keep track of it
	bool negative[2] = { false, false };
	unsigned long long* num_copy = new unsigned long long[num.digits.size()](); //create a copy of num in case it's polarity needs to be reversed
	for (int i = 0; i < num.digits.size(); i++)
	{
		num_copy[i] = *num.digits[i];
	}
	if (*(this->digits.back()) >> 63)
	{
		twosComplement(*this);
		negative[0] = true;
	}
	if (*num.digits.back() >> 63)
	{
		int k = num.digits.size() - 1;
		for (; k >= 0; k--) num_copy[k] = ~num_copy[k];
		k++; //set k back to 0 after loop
		while (num_copy[k] == 0xFFFFFFFFFFFFFFFF)
		{
			num_copy[k] = 0;
			k++;
		}
		
		num_copy[k] += 1;
		negative[1] = true;
	}

	//we need to resize *this so that the multiplication will fit into it, the new size will be equal to the number of 64-bit words in
	//*this added to the number of 64-bit words in num
	int start = this->digits.size() - 1;
	for (int i = 0; i < num.digits.size(); i++)
	{
		unsigned long long* new_word = new unsigned long long;
		*new_word = 0;
		this->digits.push_back(new_word);
	}

	//were eventually going to be overwriting the existing words in *this with our partial multiplications, so we need to work backwards from the
	//end of *this and multiply all 32-bit sub numbers with all 32-bit sub numbers of num. The last multiplication from a word in *this will end up
	//overwriting itself which is ok

	for (int i = start; i >= 0; i--)
	{
		for (int j = num.digits.size() - 1; j >= 0; j--)
		{
			//inside this n loop is where each Karatsuba multiplication takes place. each multiplication that takes place inside
			//the loop will have the same index so no shifting needs to take place until this loop terminates.
			a = *(this->digits[i]) >> 32;
			b = *(this->digits[i]) & 0x00000000FFFFFFFF;
			c = num_copy[j] >> 32;
			d = num_copy[j] & 0x00000000FFFFFFFF;

			z2 = a * c;
			z0 = b * d;
			z1 = (b - a) * (c - d) + z2 + z0; //z1 is a signed variable so there should be no issue dealing with negative numbers

			//since we're only dealing with positive numbers then every partial multiplication should be positive so we should do unsigned additions.
			//TODO: It may actually be possible for z1 to be negative, if things start acting funny see if this is happening and add a signed addition for it

			//when adding z2 and z0 to their respective totals an unsigned addition takes place because there are only positive numbers.
			//z1 will be added normally as it may be positive or negative
			this->partialAddition(z2, i + j + 1);
			this->partialAddition(z1 >> 32, i + j + 1);

			//If *this is going to overwrite itself then set the word equal to z1 << 32 instead of add z1 << 32
			if (j == 0) *(this->digits[i]) = z1 << 32;
			else this->partialAddition(z1 << 32, i + j);

			this->partialAddition(z0, i + j);
		}
	}

	//if *this or num was negative, but not both, then *this needs to be inverted
	if (negative[0] ^ negative[1]) twosComplement(*this);

	//add a loop here about removing leading zeros and then multiplication is done. Since we're only multiplying positive numbers just need to look for 0
	for (int i = this->digits.size() - 1; i > 0; i--)
	{
		if ((*(this->digits[i]) == 0) || (*(this->digits[i]) == -1))
		{
			if ((*(this->digits[i - 1]) >> 63) ^ (*(this->digits[i]) >> 63)) break;
			delete this->digits[i];
			this->digits.erase(this->digits.end() - 1);
		}
		else break;
	}

	delete[] num_copy;
	return *this;
}
int_64x& int_64x::operator*=(int num)
{
	//The same algorithm as the other *= operator but is quicker because it multiplies an int_64x with an
	//unsigned long long. The largest size for that is 64-bits so it removes the need for some of the loops
	//and other various things in the other operator.
	unsigned long long* array_one = new unsigned long long[2 * this->digits.size()]();
	unsigned long long  array_two[2] = { num & 0x00000000FFFFFFFF, num >> 32 };

	//Instead of dealing with negative numbers, just flip everything to be positive and the flip back at the
	//end of the multiplication
	bool flipped[2] = { false, false };

	if (*(this->digits.back()) & 0x8000000000000000)
	{
		//since this number is negative flip it
		twosComplement(*this);
		flipped[0] = true;
	}
	if (num & 0x8000000000000000)
	{
		//since this number is negative flip it
		num = ~num + 1;
		flipped[1] = true;
	}

	//the bits will be captured in order from least significant to most significant, however, each 32 bit piece
	//will maintain it's BigEndianness
	for (int i = 0; i < this->digits.size(); i++)
	{
		array_one[2 * i] = (*(this->digits[i]) & 0x00000000FFFFFFFF);
		array_one[2 * i + 1] = (*(this->digits[i]) >> 32);

	}

	//Increase the lenght of *this by one so the multiplication will fit
	int this_original_size = this->digits.size();
	unsigned long long* new_word = new unsigned long long;
	*new_word = 0; //will be adding so make new words equal to 0
	this->digits.push_back(new_word);

	//also before updating *this, update it's current value to 0 for addition
	for (int i = 0; i < this_original_size; i++) *(this->digits[i]) = 0;

	unsigned long long num1, num2;
	for (int i = 0; i < (2 * this_original_size); i++)
	{
		//first we multiply array_one[i] by the first 32 bits of num
		num1 = array_one[i] * array_two[0];
		num2 = array_one[i] * array_two[1];

		if ((i % 2) == 0)
		{
			this->partialAddition(num1, i / 2);
			this->partialAddition(num2 << 32, (i + 1) / 2);
			this->partialAddition(num2 >> 32, (i + 1) / 2 + 1);
		}
		else
		{
			this->partialAddition(num1 << 32, i / 2);
			this->partialAddition(num1 >> 32, i / 2 + 1);
			this->partialAddition(num2, (i + 1) / 2);
		}

		//then we multiply by the second 32 bits of num
	}

	//Make sure to free up memory from the temporary arrays
	delete[] array_one;

	//The multiplication is complete, however, negative numbers need to be flipped back
	if (flipped[0] && flipped[1])
	{
		//no need to flip the answer but num needs to be flipped back to its original state
		num = ~num + 1;
	}
	else if (flipped[0])
	{
		//only one flip occured so the answer needs to be flipped, num wasn't flipped in this
		//case so no need to flip it
		twosComplement(*this);
	}
	else if (flipped[1])
	{
		//only one flip occured so the answer needs to be flipped, and it was num that was
		//flipped originall so it needs to be flipped back
		num = ~num + 1;
		twosComplement(*this);
	}

	//no flipping occured so just leave *this and num as they are

	//the final thing to do is to remove any unnecessary leading 1's or 0's. We don't have to check for size here because multiplying
	//two 1-word numbers together will always give a 2-word number at a minimum.
	if ((*(this->digits.back()) == 0) && !(*(this->digits[this->digits.size() - 2]) & 0x8000000000000000))
	{
		delete this->digits.back();
		this->digits.erase(this->digits.end() - 1);
	}
	else if ((*(this->digits.back()) == 0xFFFFFFFFFFFFFFFF) && (*(this->digits[this->digits.size() - 2]) & 0x8000000000000000))
	{
		delete this->digits.back();
		this->digits.erase(this->digits.end() - 1);
	}

	return *this;
}
int_64x operator*(int_64x& num1, int_64x& num2)
{
	//TODO:
	//this function creates a copy of num1 which gets destroyed when the function ends. since this function needs to return
	//an int_64x, it creates a copy of num1 before its destroyed and returns that. Is there a way around this double copy?
	//std::cout << "* operator called." << std::endl;
	int_64x num1_copy = num1;
	num1_copy *= num2;
	//std::cout << "*= of num1 is complete." << std::endl;
	return num1_copy;

	//return (num1 *= num2);
}
int_64x operator*(int_64x num1, int num2)
{
	num1 *= num2;
	return num1;
}
void int_64x::FastMultiplication(const int_64x& num)
{
	//this function is called when multiplying two numbers that are relatively small. Copies of both num and *this are
	//created on the stack so that accessing memory will be quicker. If both num and *this are 50 words long or less then
	//this function will be used for multiplication. This function needs 1.6kB of stack memory to operate, however, the
	//memory is freed up as soon as the multiplication is complete so this function can be called any number of times in
	//the same application.

	unsigned long long z2, z0, a, b, c, d; //these variables hold the value of the partial multiplications at each iteration
	long long z1; //z1 has the potential to become negative so make it a signed long long

	//first we create copies of *this and num on the stack in fixed arrays of 50 elements and we create a container for their
	//multiplication which is 100 elements
	unsigned long long this_copy[50] = { 0 }, num_copy[50] = { 0 }, ans[100] = { 0 };
	for (int i = 0; i < this->digits.size(); i++) this_copy[i] = *(this->digits[i]);
	for (int i = 0; i < num.digits.size(); i++) num_copy[i] = *num.digits[i];

	//if *this or num are negative flip them and keep track of it
	bool negative[2] = { false, false };
	
	if (*(this->digits.back()) >> 63)
	{
		int k = this->digits.size() - 1;
		for (; k >= 0; k--) this_copy[k] = ~this_copy[k];
		k++; //set k back to 0 after loop
		while (this_copy[k] == 0xFFFFFFFFFFFFFFFF)
		{
			this_copy[k] = 0;
			k++;
		}

		this_copy[k] += 1;
		negative[0] = true;
	}
	if (*num.digits.back() >> 63)
	{
		int k = num.digits.size() - 1;
		for (; k >= 0; k--) num_copy[k] = ~num_copy[k];
		k++; //set k back to 0 after loop
		while (num_copy[k] == 0xFFFFFFFFFFFFFFFF)
		{
			num_copy[k] = 0;
			k++;
		}

		num_copy[k] += 1;
		negative[1] = true;
	}
	
	//we need to resize *this so that the multiplication will fit into it, the new size will be equal to the number of 64-bit words in
	//*this added to the number of 64-bit words in num
	int start = this->digits.size() - 1;
	for (int i = start; i >= 0; i--)
	{
		for (int j = num.digits.size() - 1; j >= 0; j--)
		{
			//inside this n loop is where each Karatsuba multiplication takes place. each multiplication that takes place inside
			//the loop will have the same index so no shifting needs to take place until this loop terminates.
			a = this_copy[i] >> 32;
			b = this_copy[i] & 0x00000000FFFFFFFF;
			c = num_copy[j] >> 32;
			d = num_copy[j] & 0x00000000FFFFFFFF;

			z2 = a * c;
			z0 = b * d;
			z1 = (b - a) * (c - d) + z2 + z0; //z1 is a signed variable so there should be no issue dealing with negative numbers

			//since we're only dealing with positive numbers then every partial multiplication should be positive so we should do unsigned additions.
			//TODO: It may actually be possible for z1 to be negative, if things start acting funny see if this is happening and add a signed addition for it

			//when adding z2 and z0 to their respective totals an unsigned addition takes place because there are only positive numbers.
			//z1 will be added normally as it may be positive or negative
			unsignedAddition(ans, z2, 100, i + j + 1);

			unsignedAddition(ans, z1 >> 32, 100, i + j + 1);
			unsignedAddition(ans, z1 << 32, 100, i + j);

			unsignedAddition(ans, z0, 100, i + j);
		}
	}
	
	//if *this or num was negative, but not both, then ans needs to be inverted to a negative number
	if (negative[0] ^ negative[1])
	{
		int k = num.digits.size() + this->digits.size() - 2; //although ans is 100 elements long, this is the amount that we've actually changed
		for (; k >= 0; k--) ans[k] = ~ans[k];
		k++; //set k back to 0 after loop
		while (ans[k] == 0xFFFFFFFFFFFFFFFF)
		{
			ans[k] = 0;
			k++;
		}

		ans[k] += 1;
		negative[1] = true;
	}

	//now we copy over all of the useful digits from num and put them into *this, num is bigger than *this so digits will need to be added to *this
	for (int i = 0; i < this->digits.size(); i++) *(this->digits[i]) = ans[i];
	int stop = this->digits.size() + num.digits.size();
	for (int i = this->digits.size(); i < stop; i++)
	{
		unsigned long long* new_word = new unsigned long long;
		*new_word = ans[i];
		this->digits.push_back(new_word);
	}

	//with the multiplication complete, do a final check to make sure there are no unnecessary leading words
	for (int i = this->digits.size() - 1; i > 0; i--)
	{
		if ((*(this->digits[i]) == 0) || (*(this->digits[i]) == -1))
		{
			if ((*(this->digits[i - 1]) >> 63) ^ (*(this->digits[i]) >> 63)) break;
			delete this->digits[i];
			this->digits.erase(this->digits.end() - 1);
		}
		else break;
	}
}

//Division Operators
int_64x& int_64x::operator/=(int_64x& num)
{
	//The concept of division here is that we wan't to left shift "num" as much as possible and have
	//it be less than *this. After doing that subtract the left shifted "num" value from *this. Repeat
	//this sequence until left shifting is no longer possible. Since each left shift represents a
	//multiplication by 2, 12 total left shifts would mean multiplication by 2^12 = 4096

	//Like we did with multiplication, if either number is negative just flip it to be positive and
	//then convert the answer back to a negative number at the end of the function. This isn't the 
	//most efficient way but it should be good enough for now.

	//Like with division of other built in types, we can't divide by zero here so the first thing we
	//have to make sure of is that num doesn't equal 0. If so then an error is thrown.
	std::vector<int> hits;
	if (num == 0)
	{
		//TODO: Need to throw some kind of exception here that breaks the program
		std::cout << "Division by 0 not possible." << std::endl;
		return *this;
	}
	else if (num == 1) return *this;
	else if (num > *this)
	{
		this->zero();
		return(*this);
	}

	//int_64x this_copy = *this, num_copy = num;
	//no need to copy num, it is going to be left shifted a decent amount, but then right shifted back to it's original position

	bool flipped[2] = { false, false };
	if (*(this->digits.back()) & 0x8000000000000000)
	{
		twosComplement(*this); //make a flipped copy of *this
		flipped[0] = true;
	}
	if (*num.digits.back() & 0x8000000000000000)
	{
		twosComplement(num); //make a flipped copy of num
		flipped[1] = true;
	}

	long long lead_bit_one = GetLeadBitLocation(*this), lead_bit_two = GetLeadBitLocation(num);
	long long shift = lead_bit_one - lead_bit_two;
	int lead_bit_two_start = lead_bit_two;

	//std::cout << lead_bit_two_start << std::endl;
	//PrintBinary(num);

	num <<= shift; //start by shifting num as far as it can go
	lead_bit_two += shift;

	while (true)
	{
		if (num > *this) //TODO: If statements can get pretty time expensive, is there some way to accomplish this without an if statement?
		{ 
			shift--;
			num >>= 1;
			lead_bit_two--;
		}
		//PrintBinary(num);

		hits.push_back(shift); //this bit will eventually be flipped in the answer
		//std::cout << shift + lead_bit_two_start << std::endl;
		*this -= num;

		//PrintBinary(*this);

		lead_bit_one = GetLeadBitLocation(*this);
		if (lead_bit_one <= lead_bit_two_start) break;

		num >>= (lead_bit_two - lead_bit_one);
		shift -= (lead_bit_two - lead_bit_one);
		lead_bit_two = lead_bit_one;
	}

	//std::cout << this->digits.size() << std::endl;
	//PrintBinary(num);

	//shift num back to it's original position
	num >>= (lead_bit_two - lead_bit_two_start);

	//PrintBinary(*this);
	//PrintBinary(num);
	//do one final check to see if the original num can go into *this
	if (num <= *this)
	{
		*this -= num;
		hits.push_back(0);
	}

	//for (int i = (this->digits.size() - 1); i > (hits[0] / 64); i--) delete this->digits[i];
	//this->digits.erase(this->digits.begin() + (hits[0] / 64) + 1, this->digits.end());

	//we need to make sure that *this is the correct size to fit the answer in it
	if (this->digits.size() - 1 > (hits[0] / 64))
	{
		//*this is too big and needs to be shrunk down
		for (int i = this->digits.size() - 1; i > (hits[0] / 64); i--) delete this->digits[i];
		this->digits.erase(this->digits.begin() + (hits[0] / 64 + 1), this->digits.end());
	}
	else if (this->digits.size() - 1 < (hits[0] / 64))
	{
		//*this is too small and needs to be increased
		int stop = (hits[0] / 64) - (this->digits.size() - 1); //don't want to use digits.size() in below loop as value will change
		for (int i = 0; i < stop; i++)
		{
			unsigned long long* new_word = new unsigned long long;
			*new_word = 0;
			this->digits.push_back(new_word);
		}
	}
	for (int i = 0; i < this->digits.size(); i++) *(this->digits[i]) = 0;

	//finally, set all of the correct bits one by one
	//TODO: I'm sure there's a more efficient way to do this
	int word = 0;
	for (int i = 0; i < hits.size(); i++)
	{
		word = hits[i] / 64;
		//std::cout << hits[i] << ", " << ((unsigned long long)0x1 << (hits[i] % 64)) << ", " << hits[i] % 64 << std::endl;
		*(this->digits[word]) |= ((unsigned long long)0x1 << (hits[i] % 64));
	}

	//we need to make sure that we haven't changed the polarity of the number by mistake. At this point both of the orignal numbers
	//were turned into positive values so if the lead bit is now a 1 then we need to add an extra '0' word to the front.
	if (*(this->digits.back()) & 0x8000000000000000)
	{
		unsigned long long* new_word = new unsigned long long;
		*new_word = 0;
		this->digits.push_back(new_word);
	}

	//finally, we need to flip the polarity of anything that was flipped at the beginning of the algorithm
	if (flipped[0] && flipped[1])
	{
		//no need to flip the answer but num needs to be flipped back to its original state
		twosComplement(num);
	}
	else if (flipped[0])
	{
		//only one flip occured so the answer needs to be flipped, num wasn't flipped in this
		//case so no need to flip it
		twosComplement(*this);
	}
	else if (flipped[1])
	{
		//only one flip occured so the answer needs to be flipped, and it was num that was
		//flipped originall so it needs to be flipped back
		twosComplement(num);
		twosComplement(*this);
	}

	return *this;
}
//int_64x& int_64x::operator/=(const unsigned long long& num)
//{
//	//when we divide an int_64x type by one of the built in types as opposed to another int_64x the process is actually
//	//much easier. Two built in division are done to every word of *this, a normal division and a modulus division. The final
//	//answer will be the sum of all of these divisions + the sum of all the modulus division divided by num. Here's an example
//	//using decimal division (although the same applies to binary division) where we don't look at anything more than a two digit
//	//number:
//	//Step 1: 12345678 / 12 -------> (12 / 12 << 6) + (34 / 12 << 4) + (56 / 12 << 2) + (78 / 12 << 0) = 1,020,406
//	//Step 2: 12345678 % 12 / 12 --> ((12 % 12 << 6) + (34 % 12 << 4) + (56 % 12 << 2) + (78 % 12 << 0)) / 12 = 8400
//	//12345678 / 12 = Step 1 + Step 2 = 1,020,406 + 8400 = 1,028,806 and this is the correct answer
//
//	//TODO: I just realized that the logic breaks down in the above example when 12 is dividing a number smaller than itself.
//	//If we used 99 in the same example instead of 12 then Step 1 would be 0 and Step 2 would be 12345678 (the starting number)
//	//and then we'd get caught in an infinite loop. In standard division you would have to look to the next digits and see how
//	//many times 99 goes into that, however, we can't do that here because of 64-bit limitations. Not sure of a way to overcome
//	//this right now (other than using the int_64x division)
//
//	//Division this way is nice because *this will remain the exact same length (although it may gain some lead zeros that it
//	//didn't have before which will need to be removed
//
//	//First create a copy of *this to hold the modular division in
//	int_64x modulus = *this;
//
//	for (int i = 0; i < this->digits.size(); i++)
//	{
//		*modulus.digits[i] = (*(this->digits[i]) % num);
//		*(this->digits[i]) = (*(this->digits[i]) / num);
//	}
//
//	if (modulus >= num)
//	{
//		modulus /= num; //recursive call to divide the modulus
//		*this += modulus;
//	}
//
//	return *this;
//}
int_64x int_64x::operator/(int_64x& num)
{
	//The concept of division here is that we wan't to left shift "num" as much as possible and have
	//it be less than *this. After doing that subtract the left shifted "num" value from *this. Repeat
	//this sequence until left shifting is no longer possible. Since each left shift represents a
	//multiplication by 2, 12 total left shifts would mean multiplication by 2^12 = 4096

	//Like we did with multiplication, if either number is negative just flip it to be positive and
	//then convert the answer back to a negative number at the end of the function. This isn't the 
	//most efficient way but it should be good enough for now.
    
	//Like with division of other built in types, we can't divide by zero here so the first thing we
	//have to make sure of is that num doesn't equal 0. If so then an error is thrown.
	int_64x answer(0);
	if (num == 0)
	{
		//TODO: Need to throw some kind of exception here that breaks the program
		std::cout << "Division by 0 not possible." << std::endl;
		return answer;
	}

	int_64x this_copy = *this, num_copy = num;
	long long lead_bit_one = GetLeadBitLocation(*this), lead_bit_two = GetLeadBitLocation(num);

	int negatives = 0;
	if (*(this->digits.back()) & 0x8000000000000000)
	{
		twosComplement(this_copy); //make a flipped copy of *this
		negatives++;
		lead_bit_one = GetLeadBitLocation(this_copy);
	}
	if (*num.digits.back() & 0x8000000000000000)
	{
		twosComplement(num_copy); //make a flipped copy of num
		negatives++;
		lead_bit_two = GetLeadBitLocation(num_copy);
	}

	long long shift = lead_bit_one - lead_bit_two;

	while (shift >= 0)
	{
		int_64x shifted = num_copy << shift;

		if (shifted > this_copy) //TODO: If statements can get pretty time expensive, is there some way to accomplish this without an if statement?
		{
			shift--;
			shifted >>= 1;
			continue;
		}

		int_64x yo((unsigned long long)1, shift);
		answer |= yo;
		this_copy -= shifted;

		shift = GetLeadBitLocation(this_copy) - lead_bit_two;
	}

	if (negatives == 1)
	{
		//if none or both of the inputs were negative then the answer is positive, however, if only one of the inputs is negative then the answer
		//will also be negative. Give the two's complement of the answer in this case
		twosComplement(answer);
	}

	return answer;
}

//BINARY OPERATORS
//Left Shift Operators
int_64x& int_64x::operator<<=(const unsigned int left_shift)
{
	//First we need to figure out how many empty words to add at the front of the current number. Then we need to see
	//how far into the word we need to split the word. In order to not lose any bits during the shift, each word has it's
	//most significant bits become the least significant bits of the next word, while its current least significant bits
	//become its most significant bits.

	//first and foremost if the shift amount is 0 then return without doing anything
	if (!left_shift) return *this;

	int new_words = ceil(left_shift / 64.0);
	int shift_amount = left_shift % 64;
	if (shift_amount == 0) shift_amount = 64; //equation below will set shift amount to 0 instead of 64, manually reset it here
	unsigned long long polarity = *(this->digits.back()) >> 63; //0 for positive and 1 for negative
	int start = this->digits.size() - 1; //signifies where we start shifting

	//add the appropriate amount of empty words to the front of *this //, 0 for positive numbers and -1 for negative numbers
	for (int i = 0; i < new_words; i++)
	{
		unsigned long long* new_word = new unsigned long long;
		*new_word = 0;
		this->digits.push_back(new_word);
	}

	//now shift all of the bits
	for (int i = 0; i <= start; i++)
	{
		*(this->digits[start + new_words - i]) |= (*(this->digits[start - i]) >> (64 - shift_amount));
		*(this->digits[start + new_words - i - 1]) = (*(this->digits[start - i]) << shift_amount);
	}

	//with all of the shifting done, 0 any words that should no longer have bits in them
	for (int i = new_words - 2; i >= 0; i--) *(this->digits[i]) = 0;

	//The shift is now complete, however, we need to check and make sure that the polarity of the original number wasn't changed.
	//With the way the algorithm is set up this should only be possible for negative numbers
	for (int i = 0; i < ((*(this->digits.back()) >> 63) ^ polarity); i++)
	{
		//Need to turn all of the lead 0's into 1's
		*(this->digits.back()) |= (0xFFFFFFFFFFFFFFFF << (GetLeadBitLocation(*this) % 64));
	}

	//Finally we check to see if there are any superfluous words at the start of the number, if so remove all that apply
	for (int i = this->digits.size() - 1; i > 0; i--)
	{
		//this loop will only activate if there are at least two words in the number
		if ((*(this->digits[i]) == 0) || (*(this->digits[i]) == -1))
		{
			//if the lead word isn't 0 or 0xFFFFFFFFFFFFFFFF then no need to remove it
			if ((*(this->digits[i]) >> 63) == (*(this->digits[i - 1]) >> 63))
			{
				delete this->digits[i];
				this->digits.erase(this->digits.end() - 1);
			}
		}
		else break;
	}

	return *this;
}
int_64x operator<<(int_64x num, const unsigned int left_shift)
{
	//define the << operator using the already defined <<= operator
	//int_64x shifted = num;
	return (num <<= left_shift);
}

//Right Shift Operators
int_64x& int_64x::operator>>=(const unsigned int right_shift)
{
	//The same concept as left shifting, but we start at the LSB and work our way towards the MSB. Since the number is getting smaller
	//we do a small check at the end of the function to see if the number can be reduced to get rid of any unnecessary lead 0's or 1's.
	//Unlike left shifting where we always add 0's to the LSB, in right shifting we add 0's if the original number is positive and 1's
	//if the original number is negative.

	//if "right_shift" equals 0 then just return a copy of the original
	if (!right_shift) return *this;

	//unlike left shifting which can grow to an arbitrary size, we can't right shift anything beyond 0 so we actually are ok with losing
	//some bits. Check to see if the amount that we're shifting is greater than the location of our lead bit, if so just set *this to 0.
	int lead_bit_location = GetLeadBitLocation(*this);
	unsigned long long negative = *(this->digits.back()) & 0x8000000000000000; //used to keep track of the number's polarity

	if (right_shift > lead_bit_location)
	{
		for (int i = this->digits.size() - 1; i > 0; i--) delete this->digits[i];
		this->digits.erase(this->digits.begin() + 1, this->digits.end());
	}

	//We first figure out the first 64-bit word that won't be entirely shifted out of existence, this is our starting point. For example
	//if we were to right shift by 64 the entire first word would dissapear, so there's no point to start there.
	int start = right_shift / 64, stop = lead_bit_location / 64;
	int shift_amount = right_shift % 64; //how far into each word digits will be shifting from current location

	//The main shift loop
	//int start = -2; //MSB has already been set so we start the loop at the second most significant word
	for (int i = start; i < stop; i++)
	{
		//First we need to capture the digits that aren't lost during the right shift
		*(this->digits[i - start]) = *(this->digits[i]) >> shift_amount;

		//Then we need to grab the digits that are going to dissapear from the word currently in front of i
		*(this->digits[i - start]) |= *(this->digits[i + 1]) << (64 - shift_amount);
	}

	//right shift the final word
	*(this->digits[stop - start]) = *(this->digits[stop]) >> shift_amount;

	//delete any necessary words from the MSB side
	for (int i = this->digits.size() - 1; i > stop - start; i--) delete this->digits[i];
	this->digits.erase(this->digits.begin() + (stop - start + 1), this->digits.end());

	//As in the left shift operation, we want to remove any unnecessary words at the front of the number and also ensure that the
	//polarity of numbers haven't been altered during the shift
	if (negative & 0x8000000000000000)
	{
		//a negative number
		*(this->digits.back()) |= (0xFFFFFFFFFFFFFFFF << GetLeadBitLocation(*this)); //add all 1's for a negative number
		//if most significant word contains all one's and the first bit of the next word is also a 1 then the first word should be deleted
		if (*(this->digits.back()) == 0xFFFFFFFFFFFFFFFF)
		{
			if (this->digits.size() > 1 && (*(this->digits[this->digits.size() - 2]) & 0x8000000000000000))
			{
				//the most significant word can be deleted in this case
				delete this->digits.back(); //first, free up the heap memory
				this->digits.erase(this->digits.end() - 1); //second, remove the element from the digit vector
			}
		}
	}
	else
	{
		if (*(this->digits.back()) == 0)
		{
			if (this->digits.size() > 1 && !(*(this->digits[this->digits.size() - 2]) & 0x8000000000000000))
			{
				//the most significant word can be deleted in this case
				delete this->digits.back(); //first, free up the heap memory
				this->digits.erase(this->digits.end() - 1); //second, remove the element from the digit vector
			}
		}
	}

	return *this;
}
int_64x operator>>(int_64x& num, const unsigned int right_shift)
{
	//define the >> operator using the already defined >>= operator
	int_64x shifted = num;
	return shifted >>= right_shift;
}

//Or Operators
int_64x& int_64x::operator |= (int_64x& num)
{
	//The |= operator is actually straightforwards for once. Just carry out a normal | operation on every word. The only
	//real caveat here is that the two numbers being compared may have a different number of 64-bit words so care needs
	//to be taken to avoid any access violations

	//First see which of the two numbers has more 64-bit words, if its "num" then *this will need to be lengthened
	if (num.digits.size() > this->digits.size())
	{
		int diff = num.digits.size() - this->digits.size(); //need an unchanging variable for our loop below
		for (int i = 0; i < diff; i++)
		{
			unsigned long long* new_word = new unsigned long long;
			*new_word = 0;
			this->digits.push_back(new_word);
		}
	}

	//if *this is bigger than num there's no issue
	for (int i = 0; i < num.digits.size(); i++) *(this->digits[i]) |= *num.digits[i];
	
	//one final thing to check for, if the *this has more words than num, and *this is positive while num is negative then
	//all of the words that *this has beyong the length of num will need to be flipped negative. This is because the shorter
	//negative number has implied (but not actually present) words that equal 0xFFFFFFFFFFFFFFFF so that it's length matches
	//that of *this

	//if num is negative and longer than or of equal length to *this, then *this will already be negative at this point and the
	//below block won't trigger
	if (!(*(this->digits.back()) & 0x8000000000000000) && (*num.digits.back() & 0x8000000000000000))
	{
		int delete_point;
		for (int i = num.digits.size(); i < this->digits.size(); i++)
		{
			delete_point = i;

			//to get to this loop *this must have a length of at least 2, so the below shouldn't trigger a violation error
			if (*(this->digits[i - 1]) & 0x8000000000000000) break;
			
			*(this->digits[i]) = 0xFFFFFFFFFFFFFFFF;
		}

		for (int i = delete_point; i < this->digits.size(); i++) delete this->digits[i];
		this->digits.erase(this->digits.begin() + delete_point, this->digits.end());
	}

	return *this;
}
int_64x operator|(int_64x num1, int_64x& num2)
{
	num1 |= num2;
	return num1;
}

//COMPARISON OPERATORS
bool operator==(const int_64x& num1, const int_64x& num2)
{
	//returns true if the numbers are identical, otherwise returns false

	//first see if the sizes are the same, no need to check if they aren't
	if (num1.digits.size() != num2.digits.size()) return false;

	for (int i = 0; i < num1.digits.size(); i++)
	{
		if (*num1.digits[i] != *num2.digits[i]) return false;
	}

	return true;
}
bool operator!=(const int_64x& num1, const int_64x& num2)
{
	//return the opposite of the == operator
	return !operator==(num1, num2);
}
bool operator<(const int_64x& num1, const int_64x& num2)
{
	//returns true if num1 is less than num2, otherwise returns false
	//First check to see if one of the numbers is negative
	unsigned long long num1_polarity = *num1.digits.back() & 0x8000000000000000;
	unsigned long long num2_polarity = *num2.digits.back() & 0x8000000000000000;
	if (num1_polarity && !num2_polarity) return true;
	else if (!num1_polarity && num2_polarity) return false;

	//the polarity is the same, next compare the number of digits
	if (num1.digits.size() < num2.digits.size()) return true;
	else if (num2.digits.size() < num1.digits.size()) return false;

	//if the polarity is the same and the length is the same then every 64-bit word is compared
	//until one is found to be less than or greater than another one. If all words are the same
	//then the numbers are equal so we return a value of false. Start with the MSB. negative numbers
	//are the opposite of positive numbers in that the smaller the binary representation the more
	//negative a number gets, but this means that the larger binary representation will then be
	//larger so nothing has to be differently for negative and positive numbers.
	for (int i = num1.digits.size() - 1; i >= 0; i--)
	{
		if (*num1.digits[i] < *num2.digits[i]) return true;
		else if (*num1.digits[i] > *num2.digits[i]) return false;
	}

	return false; //the numbers are equal to eachother
}
bool operator>(const int_64x& num1, const int_64x& num2)
{
	//switch the numbers and then do the less than function
	return operator<(num2, num1);
}
bool operator<=(const int_64x& num1, const int_64x& num2)
{
	//return the opposite of the greater than function
	return !operator>(num1, num2);
}
bool operator>=(const int_64x& num1, const int_64x& num2)
{
	//return the opposite of the less than function
	return !operator<(num1, num2);
}

int_64x& int_64x::operator=(const int_64x& num)
{
	//std::cout << "Equal assignment operator called." << std::endl;
	//check for mistaken self assignment
	if (this == &num) return *this;
	
	//we want to delete everything in the digits vector of this and then create new pointers and fill those
	//with the values pointed to by num's pointers
	for (int i = 0; i < this->digits.size(); i++) delete this->digits[i];
	this->digits.clear();

	//iterate through all of the elements of num.digits and copy the dereferenced pointers, not the pointers themselves
	for (int i = 0; i < num.digits.size(); i++)
	{
		unsigned long long* new_num = new unsigned long long;
		*new_num = *num.digits[i];
		this->digits.push_back(new_num);
	}

	return *this;
}

//HELPER FUNCTIONS
bool CompareArraySize(int* one, int* two, int elements)
{
	//returns true if the number represented by one is larger than two, returns false if it's equal or less than two
	int one_length = 0, two_length = 0;

	for (int i = elements; i > 0; i--)
	{
		if (one_length && two_length) break; //stop the count when the starting digit of each number is found
		if (!one_length && *(one + i - 1)) one_length = i;
		if (!two_length && *(two + i - 1)) two_length = i;
	}

	if (one_length > two_length) return true;
	else if (two_length > one_length) return false;
	else
	{
		for (int i = one_length; i > 0; i--)
		{
			if (*(one + i - 1) > * (two + i - 1)) return true;
			else if (*(two + i - 1) > * (one + i - 1)) return false;
		}
	}
	return false; //all digits have the same value
}
void MultiplyArrayByTwo(int* numbers, int elements)
{
	//takes an array of uint8_t and multiplies every element by 2, if there's any carry it gets added to the next digit
	//this function is essentially for multiplying very large numbers by 2. When multiplying by 2 the carry should never
	//get larger than 1 because 2 * 9 = 18 + 1 = 19 / 10 = 1
	int carry = 0;
	for (int i = 0; i < elements; i++)
	{
		*(numbers + i) *= 2;
		*(numbers + i) += carry;
		if (*(numbers + i) >= 10)
		{
			*(numbers + i) %= 10;
			carry = 1;
		}
		else carry = 0;
	}
}
void DivideArrayByTwo(int* numbers, int elements)
{
	//takes a large number represented by an array and divides it by 2
	//The array is just a representation of each base ten digit in the number with the least significant numbers first
	//i.e. the number 12345 would be stored in the following array [5, 4, 3, 2, 1]
	//similar to integer division, if the number isn't divisible by 2 then the function will return floor(n/2)
	int carry_num = 0, current_num, start_num = elements;

	//first calculate the starting number so that any leading zeros in the array are ignored
	for (int i = elements; i > 0; i--) //after 0 'i' will wrap back around to 255, stop at zero and shift i down by 1 inside loop
	{
		if (*(numbers + i - 1))
		{
			start_num = i;
			break;
		}
	}

	for (int i = start_num; i > 0; i--) //after 0 'i' will wrap back around to 255, stop at zero and shift i down by 1 inside loop
	{
		current_num = *(numbers + i - 1) + carry_num;
		if (current_num < 2)
		{
			*(numbers + i - 1) = 0;
			carry_num = 10 * current_num;
		}
		else
		{
			if (current_num & 1) //number is odd
			{
				*(numbers + i - 1) = ((current_num - 1) / 2);
				carry_num = 10;
			}
			else //number is even
			{
				*(numbers + i - 1) = (current_num / 2);
				carry_num = 0;
			}
		}
	}
}
void AddArrays(int* one, int* two, int elements)
{
	//takes two arrays of the same size and adds the digits of array two to array one
	//similar to the above multiplyArrayByTwo() function, the carry can't be larger than 1 because 9 + 9 = 18 + existing_carry = 19 / 10 = 1

	//first figure out if addition needs to stop early, i.e. the length of the array is greater than the actual number of elements in the array
	int end_point = elements;
	for (int i = elements; i > 0; i--)
	{
		if (*(one + i - 1) || *(two + i - 1)) //as soon as a non-zero is found that's our starting location
		{
			end_point = i;
			break;
		}
	}

	int carry = 0;
	for (int i = 0; i <= end_point; i++)
	{
		*(one + i) += *(two + i);
		*(one + i) += carry;
		if (*(one + i) >= 10)
		{
			*(one + i) %= 10;
			carry = 1;
		}
		else carry = 0;
	}
}
void SubtractArrays(int* one, int* two, int elements)
{
	//takes two arrays of the same size and subtracts the digits of array two from array one
	int ending_point = elements;
	bool ending_found = false;

	//first check and make sure that array one is larger than array two, if not then return without doing anything
	for (int i = elements; i > 0; i--)
	{
		if (*(one + i - 1) || *(two + i - 1)) //look for first non-zero elements
		{
			ending_found = true; //don't want to move our end point by mistake of a pair of non-leading zeros is found
			if (*(two + i - 1) > * (one + i - 1))
			{
				std::cout << "Array two must be smaller than array one." << std::endl;
				return;
			}
			else if (*(two + i - 1) < *(one + i - 1)) break;
		}
		else
		{
			if (!ending_found) ending_point--;
		}
	}

	for (int i = 0; i <= ending_point - 1; i++)
	{
		if (*(two + i) > * (one + i))
		{
			*(one + i) = 10 + *(one + i) - *(two + i);
			if (*(one + i + 1) == 0)
			{
				int j = 1;
				while (*(one + i + j) == 0)
				{
					if (j + i > ending_point) break;
					*(one + i + j) = 9; //make sure to turn any 0's into 10's otherwise it will wrap around to 255
					j++;
				}
				*(one + i + j) -= 1;
			}
			else *(one + i + 1) -= 1;
		}
		else *(one + i) -= *(two + i);
	}
}
void PrintBinary(int_64x num)
{
	//prints the binary representation of "num"
	//std::vector<unsigned long long*> digs = num.digits;
	for (int i = num.digits.size() - 1; i >= 0; i--) std::cout << std::bitset<64>(*num.digits[i]) << std::endl;
	std::cout << std::endl;
}
std::ostream& operator<<(std::ostream& os, int_64x& num)
{
	//I can't really think of a good way to convert the current binary representation of the number to base 10 without doing a lot of addition.
	//Because of this it won't be super efficient to print out a number, however, all mathematics can still happen in binary form so at least that
	//will be quick. Most likely there won't be a need to print out hundreds of int_64x types at the same time.

	//first check to see if the number is negative, if so then print a negative symbol, then take the two's complement of the number and print that
	if (*num.digits.back() & 0x8000000000000000)
	{
		os << '-';
		int_64x flipped = flipBits(num);
		os << flipped;
		return os;
	}

	//if there's only 1 unsigned long long int then print as normal as the compiler can handle 64-bit operations
	//std::vector<unsigned long long*> digs = num.getDigits();
	if (num.digits.size() == 1) return os << *num.digits[0];

	//We don't currently know how many digits we need as this value isn't saved, so figure it out logarithmically.
	int number_length = 64 * num.digits.size() * log10(2) + 1; //add 1 just for a little buffer
	
	//Create a temporary array to store each number that needs to be printed
	//also, once we get to numbers greater than 2^64, the physical digits of 2^x will also need to be saved so that we
	//know what to multiply by
	int* base_ten = new int[number_length](), * two_power = new int[number_length]();
	two_power[0] = 1; //2^0 == 1
	int count = 0; //keeps track of which for of digits we're looking at
	unsigned long long location = 0x1;
	int current_power = 0; //represents the current exponent of two_power (i.e. 2^current_power = two_power array)

	while (current_power < (64 * num.digits.size() - 1)) //we don't count the lead bit as it represents the number's sign
	{
		if (location & *num.digits[count])
		{
			//we have a hit so add the appropriate power of 2 to the deicmal array
			AddArrays(base_ten, two_power, number_length);
		}

		MultiplyArrayByTwo(two_power, number_length);
		current_power++;

		if (current_power % 64 == 0)
		{
			//we need to move to the next unsigned long long
			count++;
			location = 0x1; //reset our location bit
		}
		else location <<= 1;
	}

	//with the base_ten array complete just save each number into a string starting from the back of the array
	//cycle through until we find the first non-zero digit before saving anything to a string
	std::string number_string = "";
	int array_position = number_length - 1;

	while (base_ten[array_position] == 0) array_position--;
	{
		//the entire array is zeros so just print out a zero
		if (array_position < 0) return os << '0';
	}
	for (int i = array_position; i >= 0; i--) number_string += base_ten[i] + 48; //adding 48 converts a number to it's associated character

	//a very long winded way of printing out a number but currently not sure what other choice I have
	return os << number_string;
}
void twosComplement(int_64x& num)
{
	//turns "num" into its own twos complement, flip the bits using the ~ operator and then add 1
	for (int i = 0; i < num.digits.size(); i++) *num.digits[i] = ~(*num.digits[i]);
	num += 1;
}
int_64x flipBits(int_64x& num)
{
	//same thing as the twosComplement() function but it creates a temporary copy of num which will
	//ultimately get erased as opposed to changing num directly
	int_64x adder(1); //TODO: need to implement a quick way to add integers to int_64x and vice versa
	int_64x flipper = num;
	for (int i = 0; i < flipper.digits.size(); i++) *flipper.digits[i] = ~(*flipper.digits[i]);
	flipper = flipper + adder;
	return flipper;
}
int fastlog2(unsigned long long value)
{
	//quickly calculates the integer portion of log2("value"). I'm not entirely sure how this function
	//works, I shamelessly took it from the internet where they mentioned that it's a DeBrujin-like
	//algorithm. I added the final part of the return value "* (0xFFFFFFFFFFFFFFFF && value)" so that
	//the function returns 0 when value is 0 instead of returning 63

	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	value |= value >> 32;
	return tab64[((uint64_t)((value - (value >> 1)) * 0x07EDD5E59A4E28C2)) >> 58] * (0xFFFFFFFFFFFFFFFF && value);
}
int GetLeadBitLocation(int_64x &num)
{
	//returns the location of the lead bit of "num", for example the binary number 1001100110 would return 9
	//this function is only intended to be used with positive int_64x types, however, it will work with negative
	//values as well(just give unexpected results).
	unsigned long long number = *num.digits.back();
	int adder = 64 * (num.digits.size() - 1);

	if (number == 0)
	{
		//In the case where lead 0's had to be added to the front of a number in order to keep it positive
		//(i.e. an unsigned number 8 would need 8 bits to create instead of 4 --> 0000 1000) we want to ignore
		//the lead digits when finding the lead bit location. The lead bit in the above case is 3 and not 7.
		if (num.digits.size() > 1)
		{
			number = *num.digits[num.digits.size() - 2];
			adder -= 64;
		}
	}
	return fastlog2(number) + adder;
}
void int_64x::partialAddition(unsigned long long num, int word)
{
	//this function is necessary to correctly carry out partial multiplication. Since we're adding bits from the middle
	//of the overall multiplication it's possible to have words where the MSB is a 1 (indicating a negative number) in
	//the middle of a positive number and vice versa. Adding this perceived negative number when it should really be
	//positve will mess up the addition. What we do instead is just ignore any kind of polarity until the very end of
	//all the partial additions.

	//the unsigned long long "num" is added to word int *this specified by "word". If "word" is larger than the length of
	//*this then an error is thrown

	if (word > this->digits.size())
	{
		std::cout << "Partial Addition Failed, number too large." << std::endl;
		return;
	}

	//carry out the addition
	*(this->digits[word]) += num;

	//check for overflow
	if ((*(this->digits[word])) < num)
	{
		int i = 1;
		while (true)
		{
			//carry over as many carry bits as necessary. the way the overall multiplication algorithm has been set up it shouldn't
			//be possible to overflow past the end of *this
			*(this->digits[word + i]) += 1;

			if (*(this->digits[word + i]) != 0) return;
			i++;
		}
	}

	//Notes / example:
	//As an example of how this function operates here's an example. int_64x a = (12345 << 120), b = 0x8000000000000000.
	//So we have a positve number and a negative number. Here's a before and after standard addition:
	//Before:
	//0000000000000000000000000000000000000000000000000000000000110000
	//0011100100000000000000000000000000000000000000000000000000000000
    //0000000000000000000000000000000000000000000000000000000000000000
	//
	//After:
	//0000000000000000000000000000000000000000000000000000000000110000
	//0011100011111111111111111111111111111111111111111111111111111111
	//1000000000000000000000000000000000000000000000000000000000000000
	//
	//The lead negative bits from b carried into the second word of a, which is correct behavior normally. Here's what a looks like
	//after a partial addition into the first word:
	//
	//After partial addition to word 0:
	//0000000000000000000000000000000000000000000000000000000000110000
	//0011100100000000000000000000000000000000000000000000000000000000
	//1000000000000000000000000000000000000000000000000000000000000000
	//
	//The second word has remained intact here which is what we want to succesfully carry out our multiplication.
}
void unsignedAddition(unsigned long long* num1, unsigned long long num2, int num1_size, int word)
{
	//same thing as the partialAddition() function, however, addition takes place in an array instead of an int_64x type
	if (word > num1_size)
	{
		std::cout << "Unsigned Addition Failed, number too large." << std::endl;
		return;
	}

	//carry out the addition
	num1[word] += num2;

	//check for overflow
	if (num1[word] < num2)
	{
		int i = 1;
		while (true)
		{
			//carry over as many carry bits as necessary.
			num1[word + i] += 1;

			if (num1[word + i] != 0) return;
			i++;
		}
	}
}
std::string int_64x::getNumberString()
{
	//Although we don't store a decimal representation of our number, it will be necessary at times to view the decimal representation.
	//This function does just that and then returns the representation as a string

	//I can't really think of a good way to convert the current binary representation of the number to base 10 without doing a lot of addition.
	//Because of this it won't be super efficient to print out a number, however, all mathematics can still happen in binary form so at least that
	//will be quick. Most likely there won't be a need to print out hundreds of int_64x types at the same time.

	//first check to see if the number is negative, if so then print a negative symbol, then take the two's complement of the number and print that
	std::string decimal = "";
	if (*(this->digits.back()) & 0x8000000000000000)
	{
		decimal += '-';
		twosComplement(*this);
		decimal += getNumberString();
		twosComplement(*this); //flip back to a positive value
		return decimal;
	}

	//if there's only 1 unsigned long long int then print as normal as the compiler can handle 64-bit operations
	//std::vector<unsigned long long*> digs = num.getDigits();
	if (this->digits.size() == 1) return std::to_string(*(this->digits[0]));

	//We don't currently know how many digits we need as this value isn't saved, so figure it out logarithmically.
	int number_length = 64 * this->digits.size() * log10(2) + 1; //add 1 just for a little buffer

	//Create a temporary array to store each number that needs to be printed
	//also, once we get to numbers greater than 2^64, the physical digits of 2^x will also need to be saved so that we
	//know what to multiply by
	int* base_ten = new int[number_length](), * two_power = new int[number_length]();
	two_power[0] = 1; //2^0 == 1
	int count = 0; //keeps track of which for of digits we're looking at
	unsigned long long location = 0x1;
	int current_power = 0; //represents the current exponent of two_power (i.e. 2^current_power = two_power array)

	while (current_power < (64 * this->digits.size() - 1)) //we don't count the lead bit as it represents the number's sign
	{
		if (location & *(this->digits[count]))
		{
			//we have a hit so add the appropriate power of 2 to the deicmal array
			AddArrays(base_ten, two_power, number_length);
		}

		MultiplyArrayByTwo(two_power, number_length);
		current_power++;

		if (current_power % 64 == 0)
		{
			//we need to move to the next unsigned long long
			count++;
			location = 0x1; //reset our location bit
		}
		else location <<= 1;
	}

	//with the base_ten array complete just save each number into a string starting from the back of the array
	//cycle through until we find the first non-zero digit before saving anything to a string
	int array_position = number_length - 1;

	while (base_ten[array_position] == 0) array_position--;
	{
		//the entire array is zeros so just print out a zero
		if (array_position < 0) return "0";
	}
	for (int i = array_position; i >= 0; i--) decimal += base_ten[i] + 48; //adding 48 converts a number to it's associated character

	//a very long winded way of printing out a number but currently not sure what other choice I have
	return decimal;
}
void int_64x::zero()
{
	//turns *this into the number 0
	if (this->digits.size() > 1)
	{
		for (int i = this->digits.size() - 1; i > 0; i--) delete this->digits[i];
		this->digits.erase(this->digits.begin() + 1, this->digits.end());
	}

	*(this->digits[0]) = 0;
}
int_64x Karatsuba(const std::vector<unsigned long long> num1, const std::vector<unsigned long long> num2, unsigned long long num1_polarity, unsigned long long num2_polarity)
{
	if ((num1.size() == 1) && (num2.size() == 1))
	{
		//this is where the multiplcation actually happens
		unsigned long long mult1 = (num1[0] & 0x00000000FFFFFFFF) * (num2[0] & 0x00000000FFFFFFFF);
		unsigned long long mult2 = (num1[0] & 0xFFFFFFFF00000000) * (num2[0] & 0x00000000FFFFFFFF);
		unsigned long long mult3 = (num1[0] & 0x00000000FFFFFFFF) * (num2[0] & 0xFFFFFFFF00000000);
		unsigned long long mult4 = (num1[0] & 0xFFFFFFFF00000000) * (num2[0] & 0xFFFFFFFF00000000);

		//initialize a new int_64x with the fourth multiplication (most signigicant word)
		int_64x ans(mult4);

		//shift ans as necessary and add in the other multiplications. Adding all of these multiplications should always yield a
		//128 bit number (although there may be a leading zero that gets truncated from it and bring it to 64-bit).
		ans <<= 32;
		ans += mult3;
		ans += mult2;
		ans <<= 32;
		ans += mult1;
		return ans;
	}
	else
	{
		//it doesn't really matter which portion of the number split has the tendency to be larger (a vs. b and c vs. d) since you
		//multiply (b - a) * (d - c) so there will always be one (long - short) and one (short - long) in the equation fo z1. Num1 
		//and Num2 are guaranteed to be the same size so splitting up the vectors isn't too dificult.
		std::vector<unsigned long long> a(num1.begin(), num1.begin() + (num1.size() / 2));
		std::vector<unsigned long long> b(num1.begin() + a.size(), num1.end());
		std::vector<unsigned long long> c(num2.begin(), num2.begin() + (num2.size() / 2));
		std::vector<unsigned long long> d(num2.begin() + c.size(), num2.end());

		//need to make sure that vectors a and c aren't empty, if so push back a 0 (should match the polarity). a is part of num1 and c
		//is part of num2
		if (a.size() == 0) a.push_back(0 - num1_polarity);
		if (c.size() == 0) c.push_back(0 - num2_polarity);

		//recursively solve the multiplication
		int_64x z2 = Karatsuba(a, c, num1_polarity, num2_polarity);
		int_64x z0 = Karatsuba(b, d, num1_polarity, num2_polarity);

		//z1 is handled a little bit differently, need to see if subtractions result in a negative number
		//TODO: need to get a vector representing (b - a) and another representing (c - d)
		std::vector<unsigned long long> ba;
		std::vector<unsigned long long> cd;
		int_64x z1 = Karatsuba(ba, cd, num1_polarity, num2_polarity) + z2 + z0; //TODO: This is currently incorrect

		z2 <<= (64 * b.size() * b.size());
		z1 <<= (64 * b.size());
		z2 += z1;
		z2 += z0;
		return z2;
	}
}
//I really liked my original algorithms for left and right shift but had to change them to accomodate for the <<= and >>= algorithms
//to work. Here's my original code for >> just so I don't lose it:

/*
int_64x int_64x::operator>>(const unsigned int right_shift)
{
	//The same concept as left shifting, but we start at the MSB and work our way towards the LSB. Since the number is getting smaller
	//we do a small check at the end of the function to see if the number can be reduced to get rid of any unnecessary lead 0's or 1's.
	//Unlike left shifting where we always add 0's to the LSB, in right shifting we add 0's if the original number is positive and 1's
	//if the original number is negative.

	//start out by copying *this into a new int_64x
	int_64x shifted = *this;

	//if "right_shift" equals 0 then just return a copy of the original
	if (!right_shift) return shifted;

	//figure out how many, if any, words from the MSB need to be deleted
	for (int i = 0; i < right_shift / 64; i++)
	{
		delete shifted.digits[shifted.digits.size() - i]; //first, free up the heap memory
		shifted.digits.erase(shifted.digits.end() - 1);   //second, remove the element from the digit vector
	}

	//We work our way from the MSB to the LSB when shifting to the right. The very first word and
	//very last word are handled slightly differently so they're done outside of the main loop
	int shift_amount = right_shift % 64; //how far into each word digits will be shifting from current location

	unsigned long long LSB;
	*shifted.digits.back() = *(this->digits.back()) >> shift_amount; //update the MSB TODO: don't use digits.back()

	//The main shift loop
	int start = -2; //MSB has already been set so we start the loop at the second most significant word
	for (int i = start; i >= (0 - shifted.digits.size()); i--)
	{
		//First we need to shift the current word so the saved LSB will fit into it
		*shifted.digits[shifted.digits.size() + i] = *(this->digits[this->digits.size() + i]) >> shift_amount;

		//Now we need to capture the digits lost in the right shift of last step and add them to the current word
		LSB = *(this->digits[this->digits.size() + i + 1]) << (64 - shift_amount);
		*shifted.digits[shifted.digits.size() + i] |= LSB;
	}

	//unlike in the left shift were we needed to check whether or not an extra word had to be added to the MSB, in the right shift we need to
	//check whether or not a word needs to be removed. Also we need to ensure that shifting of negative numbers has happened correctly.
	//TODO: I don't really like how many if statements are needed below, come back here at some point and see if there's a better way to
	//implement the below logic
	if (*(this->digits.back()) & 0x8000000000000000)
	{
		//a negative number
		*shifted.digits.back() |= (0xFFFFFFFFFFFFFFFF << GetLeadBitLocation(shifted)); //add all 1's for a negative number
		//if most significant word contains all one's and the first bit of the next word is also a 1 then the first word should be deleted
		if (*shifted.digits.back() == 0xFFFFFFFFFFFFFFFF)
		{
			if (shifted.digits.size() > 1 && (*shifted.digits[shifted.digits.size() - 2] & 0x8000000000000000))
			{
				//the most significant word can be deleted in this case
				delete shifted.digits.back(); //first, free up the heap memory
				shifted.digits.erase(shifted.digits.end() - 1); //second, remove the element from the digit vector
			}
		}
	}
	else
	{
		if (*shifted.digits.back() == 0)
		{
			if (shifted.digits.size() > 1 && !(*shifted.digits[shifted.digits.size() - 2] & 0x8000000000000000))
			{
				//the most significant word can be deleted in this case
				delete shifted.digits.back(); //first, free up the heap memory
				shifted.digits.erase(shifted.digits.end() - 1); //second, remove the element from the digit vector
			}
		}
	}

	return shifted;
}
*/

//Original Addition Algorithm, this has been proven to work, although I changed it to make addition quicker.
//The new algorithm hasn't been tested for 100% accuracy yet so keeping this code

//int_64x& int_64x::operator+=(const int_64x& num)
//{
//	//will add together 64-bit numbers together as normal, I don't think I can come up with any kind of quicker addition algorithm then what
//	//already exists. After adding I check to see if the first 64 bits have gotten smaller than their original size. This is only possible if
//	//there's been an overflow. In addition a maximum of 1 bit can overflow, so in this case just add 1 to the next 64 bit number and do addition
//	//as normal.
//
//	//The first thing we need to do is keep a note of whether *this is a positve or negative number as this fact will be lost after initial addition
//	bool negative = *(this->digits.back()) & 0x8000000000000000;
//	int adder = 0;
//	if (*num.digits.back() & 0x8000000000000000) adder = 0xFFFFFFFFFFFFFFFF;
//	int num_size = num.digits.size(), this_size = this->digits.size();
//	int diff = num_size - this_size, carry = 0; //carry keeps track of overflow between addition of individual 64-bit words
//
//	//if num has more words than *this we need to add empty words so that the lenghts are the same
//	for (int i = 0; i < diff; i++)
//	{
//		unsigned long long* new_word = new unsigned long long;
//		*new_word = 0;
//		this->digits.push_back(new_word);
//		this_size++;
//	}
//
//	//add all of the 64-bit words from num to the 64-bit words from *this
//	for (int i = 0; i < num_size; i++)
//	{
//		//first add carry bit...
//		*(this->digits[i]) += carry;
//
//		//it's unlikely, however, possible that this carry bit can cause overflow on it's own (when digits[i] == 0xFFFFFFFFFFFFFFFF)
//		//we need to check and make sure this hasn't happened. If the carry bit caused overflow then digits[i] will now have a value of
//		//0. If we cast digits[i] to a boolean value then the only thing that can make it 0 is 0, anything else will turn into a one.
//		//It's also possible that digits[i] = 0 and carry was also 0, so we neeed to make sure we don't erroneously add a carry if digits[i]
//		//was 0 to begin with. This can be fixed by a boolean check of the current value of carry. 0 && !(0) is 0 while 1 && !(0) is 1.
//		carry = carry && (!((bool)*(this->digits[i]))); //carry will be set to 0 here if overflow bit didn't cause another overflow
//
//		//... and then add the two numbers together
//		*(this->digits[i]) += *num.digits[i];
//
//		//need to check for overflow once agin. The addition will have overflowed if the new value of digits[i] is less than the value
//		//of num.digits[i]. If digits[i] is greater than or equal to num.digits[i] the addition was valid. Even with the largest overflow
//		//possible (adding a value of 0xFFFFFFFFFFFFFFFF) digits[i] will be 1 less than it's current value. If you add 0 to digits[i] it
//		//will be equal to itself which is valid.
//
//		//TODO: Come back to here at some point and try to find a way to check this wihtout an if statement
//		if (*(this->digits[i]) < *num.digits[i]) carry = 1;
//	}
//
//	//in the case that *this has more words than num we need another loop to handle any more potential carry over. This loop will only
//	//commence if *this has more words than num. If num is negative we also need to add 0xFFFFFFFFFFFFFFFF during each iteration
//	for (int i = 0; i < (this_size - num_size); i++)
//	{
//		//same as the loop above except the value added from num will either be 0 or 0xFFFFFFFFFFFFFFFF
//		*(this->digits[num_size + i]) += carry;
//		carry = carry && (!((bool)*(this->digits[num_size + i])));
//		*(this->digits[num_size + i]) += adder;
//		if (*(this->digits[num_size + i]) < adder) carry = 1;
//	}
//
//	//Now we need to see if ovflow has happened in the most significant 64 bits. Since the MSB here represents + or - and isn't part of the
//	//actual number the overflow is handled a little differently depending on whether the orignal numbers were positve or negative. These cases
//	//are handled separately.
//
//	//Case 1: Overflow from Addition of two positive numbers.
//	//An overflow will occur if we add two positive numbers together and the MSB get's turned into a 1 (which signifies a negative number). All of the
//	//binary digits should actually be correct in this case, we just need to add a new empty unsigned long long to the front of the number to ensure
//	//that the lead digit is a zero and our number stays positive
//	if ((!negative) & (!(bool)(*num.digits.back() & 0x8000000000000000)))
//	{
//		if (*(this->digits.back()) & 0x8000000000000000)
//		{
//			//overflow has occured, add a new empty 64-bit word to front of number
//			unsigned long long* zero = new unsigned long long;
//			*zero = 0;
//			this->digits.push_back(zero);
//		}
//		return *this;
//	}
//
//	//Case 2: Overflow from Addition of two negative numbers
//	//Negative numbers are a little bit trickier here as adding them will always cause an overflow in the traditional sense because the MSB is always a 1.
//	//For example, -1 + -1 looks like 1111 + 1111 = 11110. The sum doesn't fit into 4 bits, however, truncating the MSB (as will happen automatically)
//	//yields 1110 which is the correct value of -2. So in this case we just need to ignore the overflow to get the correct number. Here's another case. -8 + -8
//	//looks like 1000 + 1000 = 10000. 10000 does indeed equal -16, however, if the MSB gets truncated here then we're left with 0000 which is no longer correct. So
//	//in one case truncating gave the correct answer and in the other truncating didn't. What we can do so the addition works in both cases is simply to do the same
//	//thing (albeit opposite) of what we did when adding two positive numbers. Check to see if the leading bit is a 0, if so add a new 64-bit word of all 1's, otherwise
//	//do nothing.
//	if (negative & ((bool)(*num.digits.back() & 0x8000000000000000)))
//	{
//		//std::cout << "Adding two negatives" << std::endl;
//		if (!(bool)(*(this->digits.back()) & 0x8000000000000000))
//		{
//			//overflow has occured, add a new empty 64-bit word to front of number
//			unsigned long long* zero = new unsigned long long;
//			*zero = 0xFFFFFFFFFFFFFFFF;
//			this->digits.push_back(zero);
//		}
//		return *this;
//	}
//
//	//Case 3: Overflow from Addition of a positive and a negative number
//	//Overflow doesn't happen in the traditional sense here. Technically you couldn't overflow at all in this case because adding a positve and negative together
//	//both makes the positve smaller (further from overflowing) and the negative bigger (again further from overflowing). Let's look at a few examples: -3 + 6
//	//1101 + 0110 = 10011. Truncating the MSB here leads to 0011 which is 3 so it's correct. -8 + 7 = 1000 + 0111 = 1111 which is -1 and correct. Indeed, no matter
//	//what way you look at it, truncating the MSB will always yield the correct answer, so if we've gotten to this point of the algorithm then just return our
//	//answer
//
//	//Before returning the answer we need to make sure there aren't any unnecessary word at the front.
//	//TODO:: I really don't like the below loops, pretty time consuming when compared to normal addition and just to make sure there aren't extra words in the front.
//	//There's got to be a more efficient check afor this. Come back to this at some point
//	if (this->digits.size() > 1)
//	{
//		if ((*(this->digits.back()) == 0) && !(*(this->digits[this->digits.size() - 2]) & 0x8000000000000000))
//		{
//			//extra word(s) of all 0's at front, delete all of these words
//			int delete_amount = 0;
//			for (int i = this->digits.size() - 1; i > 0; i--)
//			{
//				if (*this->digits[i] == 0)
//				{
//					delete this->digits[i]; //free up memory from last element
//					delete_amount++;
//				}
//				else break;
//			}
//			
//			this->digits.erase(this->digits.end() - delete_amount, this->digits.end()); //remove last element from vector
//		}
//		else if ((*(this->digits.back()) == 0xFFFFFFFFFFFFFFFF) && (*(this->digits[this->digits.size() - 2]) & 0x8000000000000000))
//		{
//			//extra word(s) of all 1's at front, delete all of these words
//			int delete_amount = 0;
//			for (int i = this->digits.size() - 1; i > 0; i--)
//			{
//				if (*this->digits[i] == 0xFFFFFFFFFFFFFFFF)
//				{
//					delete this->digits[i]; //free up memory from last element
//					delete_amount++;
//				}
//				else break;
//			}
//			this->digits.erase(this->digits.end() - delete_amount, this->digits.end()); //remove last element from vector
//		}
//	}
//	return *this;
//}

//Original Multiplication algorithm

//int_64x& int_64x::operator*=(int_64x& num)
//{
//	//To multiply two int_64x types the first thing we need to do is break down all of the 64-bit building
//	//blocks of the two numbers into 32-bits of information, but store them in unsigned long longs so no
//	//data is lost upon multiplication. This is because the maximum value from multiplication of 2 32-bit
//	//numbers is a 64-bit number.
//	unsigned long long* array_one = new unsigned long long[2 * this->digits.size()]();
//	unsigned long long* array_two = new unsigned long long[2 * num.digits.size()]();
//
//	//Instead of dealing with negative numbers, just flip everything to be positive and the flip back at the
//	//end of the multiplication
//	bool flipped[2] = { false, false };
//
//	if (*(this->digits.back()) & 0x8000000000000000)
//	{
//		//since this number is negative flip it
//		twosComplement(*this);
//		flipped[0] = true;
//	}
//	if (*num.digits.back() & 0x8000000000000000)
//	{
//		//since this number is negative create an inverse copy and set the digit pointer
//		twosComplement(num);
//		flipped[1] = true;
//	}
//
//	//the bits will be captured in order from least significant to most significant, however, each 32 bit piece
//	//will maintain it's BigEndianness
//	for (int i = 0; i < this->digits.size(); i++)
//	{
//		array_one[2 * i] = (*(this->digits[i]) & 0x00000000FFFFFFFF);
//		array_one[2 * i + 1] = (*(this->digits[i]) >> 32);
//
//	}
//
//	for (int i = 0; i < num.digits.size(); i++)
//	{
//		array_two[2 * i] = (*num.digits[i] & 0x00000000FFFFFFFF);
//		array_two[2 * i + 1] = (*num.digits[i] >> 32);
//
//	}
//
//	//We need to resize *this so that the multiplication will fit into it. The maximum amount of 64-bit words will be equal to
//	//the total amount of 64 bit-words being multiplied (i.e. if *this has 3 words and num has 2 then the result will have a 
//	//maximum of 5 64-bit words. Using an 8 bit example, the three 8-bit-word number 5,280,869 [01010000 10010100 01100101] *
//	//the two 8-bit-word number 5,221 [00010100 01100101] = the five 8-bit-word number 27,571,417,049 [00000110 01101011
//	//01100010 01101111 11011001]. This logic holds true for multiplying 64-bit words as much as 8 bit-words). Since the total
//	//amount of words need is equal to this.size() + num.size(), and *this already has this.size() words in it, we just need to
//	//add num.size() words at the end of *this to hold the multiplcation
//
//	//before increasing the capacity of *this, save the current length as it will be needed in the below loops
//	int this_original_size = this->digits.size();
//	for (int i = 0; i < num.digits.size(); i++)
//	{
//		unsigned long long* new_word = new unsigned long long;
//		*new_word = 0; //will be adding so make new words equal to 0
//		this->digits.push_back(new_word);
//	}
//
//	//also before updating *this, update it's current value to 0 for addition
//	for (int i = 0; i < this_original_size; i++) *(this->digits[i]) = 0;
//
//	unsigned long long full;
//	for (int i = 0; i < (2 * this_original_size); i++)
//	{
//		for (int j = 0; j < (2 * num.digits.size()); j++)
//		{
//			//Carry out partial additions on *this. If (i + j) is an even number we add the word as is (this is
//			//because the number of shifts on the multiplicatino will be 0 % 64), and if (i + j) is odd then it
//			//needs to be split into separate additions. The least significant 32 bits will go into word (i + j) / 2
//			//the most significatn 32 bits go into word (i + j) / 2 + 1.
//			full = array_one[i] * array_two[j];
//
//			if ((i + j) % 2 == 0) this->partialAddition(full, (i + j) / 2);
//			else
//			{
//				this->partialAddition(full << 32, (i + j) / 2);
//				this->partialAddition(full >> 32, (i + j) / 2 + 1);
//			}
//		}
//	}
//
//	//Make sure to free up memory from the temporary arrays
//	delete[] array_one;
//	delete[] array_two;
//
//	//The multiplication is complete, however, negative numbers need to be flipped back
//	if (flipped[0] && flipped[1])
//	{
//		//no need to flip the answer but num needs to be flipped back to its original state 
//		twosComplement(num);
//	}
//	else if (flipped[0])
//	{
//		//only one flip occured so the answer needs to be flipped, num wasn't flipped in this
//		//case so no need to flip it
//		twosComplement(*this);
//	}
//	else if (flipped[1])
//	{
//		//only one flip occured so the answer needs to be flipped, and it was num that was
//		//flipped originall so it needs to be flipped back
//		twosComplement(num);
//		twosComplement(*this);
//	}
//
//	//no flipping occured so just leave *this and num as they are
//
//	//the final thing to do is to remove any unnecessary leading 1's or 0's. We don't have to check for size here because multiplying
//	//two 1-word numbers together will always give a 2-word number at a minimum.
//	if ((*(this->digits.back()) == 0) && !(*(this->digits[this->digits.size() - 2]) & 0x8000000000000000))
//	{
//		delete this->digits.back();
//		this->digits.erase(this->digits.end() - 1);
//	}
//	else if ((*(this->digits.back()) == 0xFFFFFFFFFFFFFFFF) && (*(this->digits[this->digits.size() - 2]) & 0x8000000000000000))
//	{
//		delete this->digits.back();
//		this->digits.erase(this->digits.end() - 1);
//	}
//
//	return *this;
//}
