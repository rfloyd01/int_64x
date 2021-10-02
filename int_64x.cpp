#include <Header_Files/pch.h>
#include <Header_Files/print.h>
#include <Header_Files/int_64x.h>
#include <iostream>
#include <bitset>
#include <cmath>

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

int_64x::int_64x(int number)
{
	//it doesn't matter whether a positive or a negative integer is passed, either can be converted directly
	//to an unsigned long long
	digits.push_back(number);
}
int_64x::int_64x(unsigned int number)
{
	//an unsigned int needs to be a positive value, however it will only take up half of the space of an unsigned
	//long, therefore we can just directly cast it
	digits.push_back(number);
}
int_64x::int_64x(long long number)
{
	//a standard long long can be positive or negative, we can just initialize the int_64x type as is
	digits.push_back(number);
}
int_64x::int_64x(unsigned long long number, int left_shift)
{
	//an unsigned long long can only be positive, therefore if the lead bit is a 1 then we need to add
	//a word of all 0's to the front of the int_64x type so that it retains its positivity.
	digits.push_back(number);
	if (number >> 63) digits.push_back(0);

	//This constructor can also be useful during the multiplication process. If a value is given to left shift
	//then the int_64x type will be left shifted by that amount upon creation. Caution should be used when 
	//initializing via left shift on a negative number. This is allowable, however, it will be casted to a positive
	//value (left shifting an already initialized negative number works just fine).
	*this <<= left_shift;
}
int_64x::int_64x(std::string number)
{
	//This is the main way to initialize an int_64x type. A string of numbers, positive or negative can be passed
	//and then converted. The only acceptable characters are the digits 0-9 and the '-' symbol, which will only
	//be accepted as the very first character in the string otherwise it's not allowed.

	//First store digits of number in an array. Then create a different array which will hold powers of two.
	//Keep multiplying the two array by two until it's larger than the number held in the other array. From here,
	//keep dividing the power of two array by 2 until it's smaller than the number array and then subtract the two array from
	//the number array. The BigInt_128 will need to be built bit-by-bit in this fashion. For now this is the quickest way
	//I can think of to construct a binary representation of a number from a string of characters.

	//funky things happen if a string of "1" is passed to this function, best to just check for this case and manually set
	//TODO: there's probably a much better way to implement this check, come back to this at some point
	if (number == "1")
	{
		digits.push_back(1);
		return;
	}
	else if (number == "-1")
	{
		digits.push_back(-1);
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
	int* base_ten = new int[number.length() + 1]();
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

	for (int i = 0; i < number_of_longs; i++) digits.push_back(0);

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
		digits[number_of_longs - 1] = num;
		number_of_longs--; //TODO: I think i can convert the above line to *digits[--number_of_longs] = num and have the same effect
		num = 0;
		power--;
	}

	//We've succesfully turned the string into a binary representation, make sure to free up the memory that we no longer need
	delete[] base_ten;
	delete[] two_power;

	//We need to make sure that any positive numbers don't have their lead bit as a 1, if so then add a 0 as the lead word of the number
	if (this->digits.back() >> 63) this->digits.push_back(0);

	//once we've set all the bits, we need to flip them and then add 1 (two's complement) if the number was defined as a negative,
	//otherwise we're done
	if (negative) twosComplement(*this);
}
int_64x::int_64x(const int_64x& num)
{
	//std::cout << "Copy constructor called." << std::endl;
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
	unsigned long long different_polarity = this->digits.back() ^ num.digits.back();
	int diff = num.digits.size() - this->digits.size(), carry = 0; //carry keeps track of overflow between addition of individual 64-bit words

	//if num has more words than *this we need to add empty words so that the lenghts are the same. The below loop will only execute if num
	//has more words than *this
	for (int i = 0; i < diff; i++) this->digits.push_back(0);

	unsigned long long this_temp, num_temp; //temporary variables are created on stack to reduce calls to heap memory
	//add all of the 64-bit words from num to the 64-bit words from *this
	for (int i = 0; i < num.digits.size(); i++)
	{
		this_temp = this->digits[i];
		num_temp = num.digits[i];
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
		this->digits[i] = this_temp;
	}

	//in the case that *this has more words than num we need another loop to handle any more potential carry over. This loop will only
	//commence if *this has more words than num. If num is negative we also need to add 0xFFFFFFFFFFFFFFFF during each iteration
	if (num.digits.back() & 0x8000000000000000)
	{
		for (int i = 0; i < (this->digits.size() - num.digits.size()); i++)
		{
			//Need to add the implied 0xFFFFFFFFFFFFFFFF at the front of the smaller negative number
			this_temp = this->digits[num.digits.size() + i];
			this_temp += carry;
			carry = carry && (!this_temp);
			this_temp += 0xFFFFFFFFFFFFFFFF;
			if (this_temp < 0xFFFFFFFFFFFFFFFF) carry = 1;
			this->digits[num.digits.size() + i] = this_temp;
		}
	}
	else
	{
		//No need to add anything extra here as we're adding to words that are 0x0000000000000000, we can break as soon as there's no carry
		if (carry)
		{
			for (int i = 0; i < (this->digits.size() - num.digits.size()); i++)
			{
				this->digits[num.digits.size() + i] += carry;
				carry = carry && (!(this->digits[num.digits.size() + i]));
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
			if ((this->digits[i] == 0) || (this->digits[i] == 0xFFFFFFFFFFFFFFFF))
			{
				if (!((this->digits[i] ^ this->digits[i - 1]) >> 63))
				{
					//polarities of final two words are the same so remove the final word
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
		if ((this->digits.back() ^ num.digits.back()) >> 63) this->digits.push_back(0 - (num.digits.back() >> 63));
		return *this;
	}
	return *this;
}
int_64x operator+(const int_64x& num1, const int_64x& num2)
{
	//define addition using the already defined += operator, create a copy of num1 inside this function instead of
	//passing a copy to the function, this reduces the amount of total copies created by 1
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

	//The first thing we need to do is compare the polarities of the two numbers, this will let us know if the sum should be larger or smaller than
	//the original number
	unsigned long long different_polarity = this->digits.back() ^ num.digits.back();
	int diff = num.digits.size() - this->digits.size(), carry = 0; //carry keeps track of overflow between addition of individual 64-bit words

	//if num has more words than *this we need to add empty words so that the lenghts are the same. The below loop will only execute if num
	//has more words than *this. Make sure to push back the appropriate word (0 for positive and -1 for negative).
	for (int i = 0; i < diff; i++) this->digits.push_back(0 - (this->digits.back() >> 63));

	unsigned long long this_temp, num_temp; //create a temporary variable on stack to reduce calls to heap memory
	//subtract all of the 64-bit words in num from the 64-bit words in *this
	for (int i = 0; i < num.digits.size(); i++)
	{
		this_temp = this->digits[i];
		num_temp = num.digits[i];
		//first remove the carry bit if it's non-zero
		if (carry)
		{
			//it's unlikely, but possible that removing the carry bit will overflow the current word, in which case digits[i] will equal 0xFFFFFFFFFFFFFFFF.
			//If digits[i] == 0xFFFFFFFFFFFFFFFF then carry remains at 1, otherwise carry is set to 0. Carry must be set before the subtraction for easier
			//formula.
			carry = (!(this_temp));
			this_temp -= 1;
		}

		//Then subtract the two numbers. If *this.digits[i] ends up increasing after the subtraction we know that overflow has occurred.
		this_temp -= num_temp;
		if (this_temp > this->digits[i]) carry = 1;
		this->digits[i] = this_temp;
	}

	//In the case that *this has more words than num we need another loop to handle any more potential carry over. This loop will only
	//commence if *this has more words than num. If num is negative we also need to add 0xFFFFFFFFFFFFFFFF during each iteration
	if (num.digits.back() & 0x8000000000000000)
	{
		for (int i = 0; i < (this->digits.size() - num.digits.size()); i++)
		{
			//Need to add the implied 0xFFFFFFFFFFFFFFFF at the front of the smaller negative number
			this_temp = this->digits[num.digits.size() + i];
			if (carry)
			{
				//it's unlikely, but possible that removing the carry bit will overflow the current word, in which case digits[i] will equal 0xFFFFFFFFFFFFFFFF.
				//If digits[i] == 0xFFFFFFFFFFFFFFFF then carry remains at 1, otherwise carry is set to 0. Carry must be set before the subtraction.
				carry = (!(this_temp));
				this_temp -= 1;
			}
			this_temp -= 0xFFFFFFFFFFFFFFFF; //TODO: Might be able to change this to + 1 for a little speedup
			if (this_temp > this->digits[num.digits.size() + i]) carry = 1;
			this->digits[num.digits.size() + i] = this_temp;
		}
	}
	else
	{
		//No need for extra subtractions as we'd be subtracting 0x0000000000000000, we can break as soon as there's no carry
		if (carry)
		{
			for (int i = 0; i < (this->digits.size() - num.digits.size()); i++)
			{
				carry = (!(this->digits[num.digits.size() + i]));
				this->digits[num.digits.size() + i] -= 1;
				if (!carry) break;
			}
		}
	}

	//The subtraction is done at this point, however, we need to either add a new word for numbers that have reversed polarity that shouldn't have,
	//or remove words from the front of the number that have become redundant (or just leave the answer as is)
	if (different_polarity >> 63)
	{
		//the original numbers had different polarities, we need to either add a new word or keep the answer the same
		if (!((this->digits.back() ^ num.digits.back()) >> 63)) this->digits.push_back(0 - !(num.digits.back() >> 63));
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
			if ((this->digits[i] == 0) || (this->digits[i] == 0xFFFFFFFFFFFFFFFF))
			{
				if (!((this->digits[i] ^ this->digits[i - 1]) >> 63))
				{
					//polarities of final two words are the same so remove the final word
					this->digits.erase(this->digits.end() - 1);
				}
				else break;
			}
		}
		return *this;
	}
	return *this;
}
int_64x operator-(const int_64x& num1, const int_64x& num2)
{
	//define subtraction using the already defined -= operator, create a copy of num1 inside this function instead of
	//passing a copy to the function, this reduces the amount of total copies created by 1
	int_64x num1_copy = num1;
	num1_copy -= num2;
	return num1_copy;
}

//Multiplication Operators
int_64x& int_64x::operator*=(const int_64x& num)
{
	//The goal here is to multiply two int_64x types using the Karatsuba multiplication method. The number of multiplications that
	//needs to be carried out is the number of 64-bit words in *this x 64-bit words in num (so when multiplying two numbers that each
	//have two 64-bit words, a total of 4 Karatsuba multiplications will take place).

	//If the numbers being multiplied are small enough we can do all the arithmetic with memory located on the stack instead of the heap
	//which ends up being ~30% quicker.
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
	for (int i = 0; i < num.digits.size(); i++) num_copy[i] = num.digits[i];
	if (this->digits.back() >> 63)
	{
		twosComplement(*this); //flip the negative number to positive
		negative[0] = true;
	}
	if (num.digits.back() >> 63)
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
	for (int i = 0; i < num.digits.size(); i++) this->digits.push_back(0);

	//were eventually going to be overwriting the existing words in *this with our partial multiplications, so we need to work backwards from the
	//end of *this and multiply all 32-bit sub numbers with all 32-bit sub numbers of num. The last multiplication from a word in *this will end up
	//overwriting itself which is ok

	for (int i = start; i >= 0; i--)
	{
		for (int j = num.digits.size() - 1; j >= 0; j--)
		{
			//inside this n loop is where each Karatsuba multiplication takes place. each multiplication that takes place inside
			//the loop will have the same index so no shifting needs to take place until this loop terminates.
			a = this->digits[i] >> 32;
			b = this->digits[i] & 0x00000000FFFFFFFF;
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
			if (j == 0) this->digits[i] = z1 << 32;
			else this->partialAddition(z1 << 32, i + j);

			this->partialAddition(z0, i + j);
		}
	}

	//if *this or num was negative, but not both, then *this needs to be inverted
	if (negative[0] ^ negative[1]) twosComplement(*this);

	//add a loop here about removing leading zeros and then multiplication is done. Since we're only multiplying positive numbers just need to look for 0
	for (int i = this->digits.size() - 1; i > 0; i--)
	{
		if ((this->digits[i] == 0) || (this->digits[i] == -1))
		{
			if ((this->digits[i - 1] >> 63) ^ (this->digits[i] >> 63)) break;
			this->digits.erase(this->digits.end() - 1);
		}
		else break;
	}

	delete[] num_copy;
	return *this;
}
int_64x operator*(const int_64x& num1, const int_64x& num2)
{
	//define multiplication using the already defined *= operator, create a copy of num1 inside this function instead of
	//passing a copy to the function, this reduces the amount of total copies created by 1
	int_64x num1_copy = num1;
	num1_copy *= num2;
	return num1_copy;
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
	for (int i = 0; i < this->digits.size(); i++) this_copy[i] = this->digits[i];
	for (int i = 0; i < num.digits.size(); i++) num_copy[i] = num.digits[i];

	//if *this or num are negative flip them and keep track of it
	bool negative[2] = { false, false };

	if (this->digits.back() >> 63)
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
	if (num.digits.back() >> 63)
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
	for (int i = 0; i < this->digits.size(); i++) this->digits[i] = ans[i];
	int stop = this->digits.size() + num.digits.size();
	for (int i = this->digits.size(); i < stop; i++) this->digits.push_back(ans[i]);

	//with the multiplication complete, do a final check to make sure there are no unnecessary leading words
	for (int i = this->digits.size() - 1; i > 0; i--)
	{
		if ((this->digits[i] == 0) || (this->digits[i] == -1))
		{
			if ((this->digits[i - 1] >> 63) ^ (this->digits[i] >> 63)) break;
			this->digits.erase(this->digits.end() - 1);
		}
		else break;
	}
}

//Division Operators
int_64x& int_64x::operator/=(const int_64x& num)
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

	//in order to keep num as a const value a copy of it is made and operated on.
	int_64x num_copy = num;

	//check to see if either number is negative, if so flip it to a positive number
	bool flipped[2] = { false, false };
	if (this->digits.back() & 0x8000000000000000)
	{
		twosComplement(*this); //make a flipped copy of *this
		flipped[0] = true;
	}
	if (num_copy.digits.back() & 0x8000000000000000)
	{
		twosComplement(num_copy); //make a flipped copy of num
		flipped[1] = true;
	}

	std::vector<int> hits;
	if (num_copy == 0)
	{
		//TODO: Need to throw some kind of exception here that breaks the program
		std::cout << "Division by 0 not possible." << std::endl;
		return *this;
	}
	else if (num_copy == 1) return *this;
	else if (num_copy > * this)
	{
		this->zero();
		return(*this);
	}

	long long lead_bit_one = GetLeadBitLocation(*this), lead_bit_two = GetLeadBitLocation(num_copy);
	long long shift = lead_bit_one - lead_bit_two;
	int lead_bit_two_start = lead_bit_two;


	num_copy <<= shift; //start by shifting num as far as it can go
	lead_bit_two += shift;

	while (true)
	{
		if (num_copy > * this)
		{
			shift--;
			num_copy >>= 1;
			lead_bit_two--;
		}

		hits.push_back(shift); //this bit will eventually be flipped in the answer
		*this -= num_copy;

		lead_bit_one = GetLeadBitLocation(*this);
		if (lead_bit_one <= lead_bit_two_start) break;

		num_copy >>= (lead_bit_two - lead_bit_one);
		shift -= (lead_bit_two - lead_bit_one);
		lead_bit_two = lead_bit_one;
	}

	//shift num back to it's original position
	num_copy >>= (lead_bit_two - lead_bit_two_start);

	//do one final check to see if the original num can go into *this
	if (num_copy <= *this)
	{
		*this -= num_copy;
		hits.push_back(0);
	}

	//we need to make sure that *this is the correct size to fit the answer in it
	if (this->digits.size() - 1 > (hits[0] / 64))
	{
		//*this is too big and needs to be shrunk down
		this->digits.erase(this->digits.begin() + (hits[0] / 64 + 1), this->digits.end());
	}
	else if (this->digits.size() - 1 < (hits[0] / 64))
	{
		//*this is too small and needs to be increased
		int stop = (hits[0] / 64) - (this->digits.size() - 1); //don't want to use digits.size() in below loop as value will change
		for (int i = 0; i < stop; i++) this->digits.push_back(0);
	}
	for (int i = 0; i < this->digits.size(); i++) this->digits[i] = 0;

	//finally, set all of the correct bits one by one
	//TODO: I'm sure there's a more efficient way to do this
	int word = 0;
	for (int i = 0; i < hits.size(); i++)
	{
		word = hits[i] / 64;
		this->digits[word] |= ((unsigned long long)0x1 << (hits[i] % 64));
	}

	//we need to make sure that we haven't changed the polarity of the number by mistake. At this point both of the orignal numbers
	//were turned into positive values so if the lead bit is now a 1 then we need to add an extra '0' word to the front.
	if (this->digits.back() & 0x8000000000000000) this->digits.push_back(0);

	//finally, we need to flip the polarity of the answer if only one of the inputs was negative
	if (flipped[0] ^ flipped[1]) twosComplement(*this);

	return *this;
}
int_64x operator/(const int_64x& num1, const int_64x& num2)
{
	//define division using the already defined /= operator, create a copy of num1 inside this function instead of
	//passing a copy to the function, this reduces the amount of total copies created by 1
	int_64x num1_copy = num1;
	num1_copy /= num2;
	return num1_copy;
}
int_64x& int_64x::operator%=(const int_64x& num)
{
	//I'm sure there's a better way to do this but for now, just use the same algorithm as division. Once the
	//division value is obtained, multiply it by "num" and return *this minus that multiplication. This should
	//work regardless of negative numbers, or if *this is smaller than num.

	//Everything between these lines is directly copied from the /= operator with the exception that functionality
	//between dividing by 1 and a number bigger than this have been switched.
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
	int_64x num_copy = num, this_copy = *this; //the actual division is applied to a copy of *this instead of *this itself

	//check to see if either number is negative, if so flip it to a positive number
	bool flipped[2] = { false, false };
	if (this_copy.digits.back() & 0x8000000000000000)
	{
		twosComplement(this_copy); //flip the copy of *this
		flipped[0] = true;
	}
	if (num_copy.digits.back() & 0x8000000000000000)
	{
		twosComplement(num_copy); //make a flipped copy of num
		flipped[1] = true;
	}

	std::vector<int> hits;
	if (num_copy == 0)
	{
		//TODO: Need to throw some kind of exception here that breaks the program
		std::cout << "Division by 0 not possible." << std::endl;
		return *this;
	}
	else if (num_copy == 1)
	{
		this->zero();
		return(*this);
	}
	else if (num_copy > this_copy) return *this;

	long long lead_bit_one = GetLeadBitLocation(this_copy), lead_bit_two = GetLeadBitLocation(num_copy);
	long long shift = lead_bit_one - lead_bit_two;
	int lead_bit_two_start = lead_bit_two;

	num_copy <<= shift;
	lead_bit_two += shift;

	while (true)
	{
		if (num_copy > this_copy)
		{
			shift--;
			num_copy >>= 1;
			lead_bit_two--;
		}

		hits.push_back(shift);
		this_copy -= num_copy;

		lead_bit_one = GetLeadBitLocation(this_copy);
		if (lead_bit_one <= lead_bit_two_start) break;

		num_copy >>= (lead_bit_two - lead_bit_one);
		shift -= (lead_bit_two - lead_bit_one);
		lead_bit_two = lead_bit_one;
	}

	num_copy >>= (lead_bit_two - lead_bit_two_start);

	if (num_copy <= this_copy)
	{
		this_copy -= num_copy;
		hits.push_back(0);
	}
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
	//The modular division function deviates slightly from standard division at this point

	if (this_copy.digits.size() - 1 > (hits[0] / 64))
	{
		this_copy.digits.erase(this_copy.digits.begin() + (hits[0] / 64 + 1), this_copy.digits.end());
	}
	else if (this_copy.digits.size() - 1 < (hits[0] / 64))
	{
		int stop = (hits[0] / 64) - (this_copy.digits.size() - 1);
		for (int i = 0; i < stop; i++) this_copy.digits.push_back(0);
	}
	for (int i = 0; i < this_copy.digits.size(); i++) this_copy.digits[i] = 0;

	int word = 0;
	for (int i = 0; i < hits.size(); i++)
	{
		word = hits[i] / 64;
		this_copy.digits[word] |= ((unsigned long long)0x1 << (hits[i] % 64));
	}

	//we need to make sure that we haven't changed the polarity of the number by mistake. At this point both of the orignal numbers
	//were turned into positive values so if the lead bit is now a 1 then we need to add an extra '0' word to the front.
	if (this_copy.digits.back() & 0x8000000000000000) this_copy.digits.push_back(0);

	//finally, we need to flip the polarity of the answer if only one of the inputs was negative
	if (flipped[0] ^ flipped[1]) twosComplement(this_copy);

	//The original division is complete, multiply the copy of *this by the original number we divided by to see how much less it is
	//than *this. The difference between the two numbers is the answer to our modular division.
	this_copy *= num;

	return (*this -= this_copy);
}
int_64x operator%(const int_64x& num1, const int_64x& num2)
{
	//define modular division using the already defined %= operator, create a copy of num1 inside this function instead of
	//passing a copy to the function, this reduces the amount of total copies created by 1
	int_64x num1_copy = num1;
	num1_copy %= num2;
	return num1_copy;
}

//Increment Operators
int_64x& int_64x::operator++()
{
	//the prefix ++ operator, like with built in types it will increase the value of *this by 1
	*this += 1;
	return *this;
}
int_64x int_64x::operator++(int)
{
	//the postfix ++ operator is defined in terms of the prefix ++ operator. A temporary variable is
	//created of *this before it gets incremented and is what's returned from this function
	int_64x temp = *this;
	operator++();
	return temp;
}
int_64x& int_64x::operator--()
{
	//the prefix -- operator, like with built in types it will decrease the value of *this by 1
	*this -= 1;
	return *this;
}
int_64x int_64x::operator--(int)
{
	//the postfix -- operator is defined in terms of the prefix -- operator. A temporary variable is
	//created of *this before it gets decremented and is what's returned from this function
	int_64x temp = *this;
	operator--();
	return temp;
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
	unsigned long shift_amount = left_shift % 64;
	unsigned long long polarity = this->digits.back() >> 63; //0 for positive and 1 for negative
	int start = this->digits.size() - 1; //signifies where we start shifting

	//if shifting by a factor of 64, just place the appropriate amount of empty words at the front of the number and return,
	//otherwise add words at the end of the number and perform the shift
	if (shift_amount == 0)
	{
		for (int i = 0; i < new_words; i++) this->digits.insert(this->digits.begin(), 0);
		return *this;
	}
	for (int i = 0; i < new_words; i++) this->digits.push_back(0);

	//now shift all of the bits
	for (int i = 0; i <= start; i++)
	{
		this->digits[start + new_words - i] |= (this->digits[start - i] >> (64 - shift_amount));
		this->digits[start + new_words - i - 1] = (this->digits[start - i] << shift_amount);
	}

	//with all of the shifting done, 0 any words that should no longer have bits in them
	for (int i = new_words - 2; i >= 0; i--) this->digits[i] = 0;

	//The shift is now complete, however, we need to check and make sure that the polarity of the original number wasn't changed.
	//With the way the algorithm is set up this should only be possible for negative numbers
	for (int i = 0; i < ((this->digits.back() >> 63) ^ polarity); i++)
	{
		//Need to turn all of the lead 0's into 1's
		this->digits.back() |= (0xFFFFFFFFFFFFFFFF << (GetLeadBitLocation(*this) % 64));
	}

	//Finally we check to see if there are any superfluous words at the start of the number, if so remove all that apply
	for (int i = this->digits.size() - 1; i > 0; i--)
	{
		//this loop will only activate if there are at least two words in the number
		if ((this->digits[i] == 0) || (this->digits[i] == -1))
		{
			//if the lead word isn't 0 or 0xFFFFFFFFFFFFFFFF then no need to remove it
			if ((this->digits[i] >> 63) == (this->digits[i - 1] >> 63)) this->digits.erase(this->digits.end() - 1);
		}
		else break;
	}

	return *this;
}
int_64x operator<<(const int_64x& num, const unsigned int left_shift)
{
	//define left shifting using the already defined <<= operator, create a copy of num inside this function instead of
	//passing a copy to the function, this reduces the amount of total copies created by 1
	int_64x num_copy = num;
	num_copy <<= left_shift;
	return num_copy;
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
	unsigned long long negative = this->digits.back() & 0x8000000000000000; //used to keep track of the number's polarity

	//if the right shift will cause the number to be zero, just make the number 0 and then return
	if (right_shift > lead_bit_location)
	{
		this->digits.erase(this->digits.begin() + 1, this->digits.end());
		this->digits[0] = 0;
		return *this;
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
		this->digits[i - start] = this->digits[i] >> shift_amount;

		//Then we need to grab the digits that are going to dissapear from the word currently in front of i
		this->digits[i - start] |= this->digits[i + 1] << (64 - shift_amount);
	}

	//right shift the final word
	this->digits[stop - start] = this->digits[stop] >> shift_amount;

	//delete any necessary words from the MSB side
	this->digits.erase(this->digits.begin() + (stop - start + 1), this->digits.end());

	//As in the left shift operation, we want to remove any unnecessary words at the front of the number and also ensure that the
	//polarity of numbers haven't been altered during the shift
	if (negative & 0x8000000000000000)
	{
		//a negative number
		this->digits.back() |= (0xFFFFFFFFFFFFFFFF << GetLeadBitLocation(*this)); //add all 1's for a negative number
		//if most significant word contains all one's and the first bit of the next word is also a 1 then the first word should be deleted
		if (this->digits.back() == 0xFFFFFFFFFFFFFFFF)
		{
			if (this->digits.size() > 1 && (this->digits[this->digits.size() - 2] & 0x8000000000000000))
			{
				//the most significant word can be deleted in this case
				this->digits.erase(this->digits.end() - 1);
			}
		}
	}
	else
	{
		if (this->digits.back() == 0)
		{
			if (this->digits.size() > 1 && !(this->digits[this->digits.size() - 2] & 0x8000000000000000))
			{
				//the most significant word can be deleted in this case
				this->digits.erase(this->digits.end() - 1);
			}
		}
	}

	return *this;
}
int_64x operator>>(const int_64x& num, const unsigned int right_shift)
{
	//define right shifting using the already defined >>= operator, create a copy of num inside this function instead of
	//passing a copy to the function, this reduces the amount of total copies created by 1
	int_64x num_copy = num;
	num_copy >>= right_shift;
	return num_copy;
}

//OR Operators
int_64x& int_64x::operator |= (const int_64x& num)
{
	//The |= operator is actually straightforwards for once. Just carry out a normal | operation on every word. The only
	//real caveat here is that the two numbers being compared may have a different number of 64-bit words so care needs
	//to be taken to avoid any access violations

	//First see which of the two numbers has more 64-bit words, if its "num" then *this will need to be lengthened
	if (num.digits.size() > this->digits.size())
	{
		int diff = num.digits.size() - this->digits.size(); //need an unchanging variable for our loop below
		for (int i = 0; i < diff; i++) this->digits.push_back(0);
	}

	//if *this is bigger than num there's no issue
	for (int i = 0; i < num.digits.size(); i++) this->digits[i] |= num.digits[i];

	//one final thing to check for, if the *this has more words than num, and *this is positive while num is negative then
	//all of the words that *this has beyong the length of num will need to be flipped negative. This is because the shorter
	//negative number has implied (but not actually present) words that equal 0xFFFFFFFFFFFFFFFF so that it's length matches
	//that of *this

	//if num is negative and longer than or of equal length to *this, then *this will already be negative at this point and the
	//below block won't trigger
	if (!(this->digits.back() & 0x8000000000000000) && (num.digits.back() & 0x8000000000000000))
	{
		int delete_point;
		for (int i = num.digits.size(); i < this->digits.size(); i++)
		{
			delete_point = i;

			//to get to this loop *this must have a length of at least 2, so the below shouldn't trigger a violation error
			if (this->digits[i - 1] & 0x8000000000000000) break;

			this->digits[i] = 0xFFFFFFFFFFFFFFFF;
		}

		this->digits.erase(this->digits.begin() + delete_point, this->digits.end());
	}

	return *this;
}
int_64x operator|(const int_64x& num1, const int_64x& num2)
{
	//define bitwise OR using the already defined |= operator, create a copy of num1 inside this function instead of
	//passing a copy to the function, this reduces the amount of total copies created by 1
	int_64x num1_copy = num1;
	num1_copy |= num2;
	return num1_copy;
}

//AND Operators
int_64x& int_64x::operator &= (const int_64x& num)
{
	//If *this has more words than num, and num is a positive number, then *this will need to be 
	//shortened so that its length is the same as num. If *this has more words and num is a negative number,
	//however, then the words beyond the length of num will remain unchanged. This is because num has implied
	//words of all 1's in front of it. Conversely, if *this is shorter than num and *this is negative, it will
	//get larger due to the implied 1's at its front.

	//Final polarity of the operation is as shown below
	//positive & positive = positive
	//negative & negative = negative
	//positive & negative = positive
	bool final_polarity = (this->digits.back() >> 63) & (num.digits.back() >> 63);

	if (num < 0)
	{
		//If num is a negative number then *this will either remain the same length or grow
		int diff = num.digits.size() - this->digits.size(); //need an unchanging variable for our loop below
		for (int i = 0; i < diff; i++) this->digits.push_back(0 - (this->digits.back() >> 63)); //if *this is shorter then it will have words added in this loop

		//*this will either have more words, or the same amount of words as num, so loop using the number of words in num
		for (int i = 0; i < num.digits.size(); i++) this->digits[i] &= num.digits[i];
	}
	else if (num > 0)
	{
		//if num is a positive number then *this will either remain the same length or shrink
		int diff = this->digits.size() - num.digits.size(); //need an unchanging variable for our loop below
		if (diff > 0) this->digits.erase(this->digits.end() - diff, this->digits.end()); //if *this is longer then it will have words removed here

		//*this will either have less words, or the same amount of words as num, so loop using the number of words in num
		for (int i = 0; i < this->digits.size(); i++) this->digits[i] &= num.digits[i];
	}
	else
	{
		//if num is 0 then the result of the & will just be 0
		this->zero();
		return *this;
	}

	//the & operation is complete, we just need to make sure that there aren't any redundant words at the front of the final number
	for (int i = this->digits.size() - 1; i > 0; i--)
	{
		if ((this->digits[i] == 0) || (this->digits[i] == -1))
		{
			if ((this->digits[i] >> 63) == (this->digits[i - 1] >> 63)) this->digits.erase(this->digits.begin() + i);
		}
	}

	return *this;
}
int_64x operator&(const int_64x& num1, const int_64x& num2)
{
	//define bitwise AND using the already defined &= operator, create a copy of num1 inside this function instead of
	//passing a copy to the function, this reduces the amount of total copies created by 1
	int_64x num1_copy = num1;
	num1_copy &= num2;
	return num1_copy;
}

//COMPARISON OPERATORS
bool operator==(const int_64x& num1, const int_64x& num2)
{
	//returns true if the numbers are identical, otherwise returns false

	//first see if the sizes are the same, no need to check if they aren't
	if (num1.digits.size() != num2.digits.size()) return false;

	for (int i = 0; i < num1.digits.size(); i++)
	{
		if (num1.digits[i] != num2.digits[i]) return false;
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
	unsigned long long num1_polarity = num1.digits.back() & 0x8000000000000000;
	unsigned long long num2_polarity = num2.digits.back() & 0x8000000000000000;
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
		if (num1.digits[i] < num2.digits[i]) return true;
		else if (num1.digits[i] > num2.digits[i]) return false;
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

//Assignment Operators
int_64x& int_64x::operator=(const int_64x& num)
{
	//check for mistaken self assignment
	if (this == &num) return *this;

	this->digits = num.digits;
	return *this;
}

//Printing Operations
std::ostream& operator<<(std::ostream& os, const int_64x& num)
{
	int_64x num_copy = num;
	std::string number_string = num_copy.getNumberString();
	return os << number_string;
}
std::string int_64x::getNumberString()
{
	//Although we don't store a decimal representation of our number, it will be necessary at times to view the decimal representation.
	//This function does just that and then returns the representation as a string

	//I can't really think of a good way to convert the current binary representation of the number to base 10 without doing a lot of addition.
	//Because of this it won't be super efficient to print out a number, however, all mathematics can still happen in binary form so at least that
	//will be quick. Most likely there won't be a need to print out hundreds of int_64x types at the same time.

	//if the int_64x doesn't have any numbers saved in it then just return a zero
	if (this->digits.size() == 0) return "0";

	//first check to see if the number is negative, if so then print a negative symbol, then take the two's complement of the number and print that
	std::string decimal = "";
	if (this->digits.back() & 0x8000000000000000)
	{
		decimal += '-';
		twosComplement(*this);
		decimal += getNumberString();
		twosComplement(*this);
		return decimal;
	}

	//if there's only 1 unsigned long long int then print as normal as the compiler can handle 64-bit operations
	//std::vector<unsigned long long*> digs = num.getDigits();
	if (this->digits.size() == 1) return std::to_string(this->digits[0]);

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
		if (location & this->digits[count])
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
void PrintBinary(int_64x num)
{
	//prints the binary representation of "num"
	//std::vector<unsigned long long*> digs = num.digits;
	for (int i = num.digits.size() - 1; i >= 0; i--) std::cout << std::bitset<64>(num.digits[i]) << std::endl;
	std::cout << std::endl;
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
void twosComplement(int_64x& num)
{
	//turns "num" into its own twos complement, flip the bits using the ~ operator and then add 1
	for (int i = 0; i < num.digits.size(); i++) num.digits[i] = ~(num.digits[i]);
	num += 1;
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
int GetLeadBitLocation(int_64x& num)
{
	//returns the location of the lead bit of "num", for example the binary number 1001100110 would return 9
	//this function is only intended to be used with positive int_64x types, however, it will work with negative
	//values as well(just give unexpected results).
	unsigned long long number = num.digits.back();
	int adder = 64 * (num.digits.size() - 1);

	if (number == 0)
	{
		//In the case where lead 0's had to be added to the front of a number in order to keep it positive
		//(i.e. an unsigned number 8 would need 8 bits to create instead of 4 --> 0000 1000) we want to ignore
		//the lead digits when finding the lead bit location. The lead bit in the above case is 3 and not 7.
		if (num.digits.size() > 1)
		{
			number = num.digits[num.digits.size() - 2];
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
	this->digits[word] += num;

	//check for overflow
	if ((this->digits[word]) < num)
	{
		int i = 1;
		while (true)
		{
			//carry over as many carry bits as necessary. the way the overall multiplication algorithm has been set up it shouldn't
			//be possible to overflow past the end of *this
			this->digits[word + i] += 1;

			if (this->digits[word + i] != 0) return;
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
void int_64x::zero()
{
	//turns *this into the number 0
	this->digits.clear();
	this->digits.push_back(0);
}
