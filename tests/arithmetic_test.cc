
#include <stdlib.h> //for rand()
#include <assert.h>

//#include <fstream>
#include <iostream>

//#include <boost/foreach.hpp>
//#include "fraction.h"
#include "../validatingbigint.h"

typedef ValidatingBigint BigInt;

using namespace std;
//typedef Fraction<BigInt> BigFrac;
#define TEST(X) { cout << ((X)?"\033[22;32mpassed":"\033[22;31mfailed") << "\033[1;30m \"" << #X <<"\" ("<< "line " << __LINE__ << ")" << endl;}

BigInt getRandom()
{
    BigInt a = 0;
    int i = rand() % 8 + 1;
    do
    {
        a = (a << 16)  | (rand() % 0xFFFF);
    } while (--i);
    
    return rand() % 2 ? a : -a;
    
}


int main(int, char** )
{
/*
    BigInt a( getRandom());
    BigInt b = 1;
    b = b << 64;
    a/b;
    return 0;*/
    BigInt(-16178750) % BigInt(-17700550);
    BigInt(-1) >> 1;
    BigInt(-2) >> 1;
    BigInt(-3) >> 1;
    BigInt(-4) >> 1;
    BigInt((int64_t)-5) >> 1;
    BigInt((int64_t)5) >> 1;
    BigInt(0) == BigInt(0);
    
    BigInt(1) << 95;
    for (int i = 0; i < 100000; i++)
    {
        BigInt a( getRandom());
        BigInt b( getRandom());
        a==b;
        a!=b;
        a< b;
        a> b;
        a<=b;
        a>=b;
        
        a==a;
        a!=a;
        a< a;
        a<=a;
        a> a;
        a>=a;
        //cout << i << "\t" << a << ", " << b << endl;
        uint32_t c = rand() % 0xFFFFFFFF;
        int d = rand() % 128;
        a>>d;
        if ((b >> d) != 0) a/(b >> d);
    
        if ( c != 0)
        {
            BigInt res = a/c;
            c*res;
            if ((b>>62) != 0)
            {
                res = a/(b >> 62);
                (b >> 62)*res;
            }
            res*(uint64_t)abs(b>>65);
        }

        //a|b;
        //a&b;
        a = a >> 1;
        b = b >> 1;
        a.toDouble();
        a+b;
        a-b;
        if (b != 0) 
        {
            a%b;
            a/b;
        }
                
        (uint64_t)(abs(a)>>64);
        a>=b;
        a = a >> 64;
        a*=a;
        
        (int64_t)(b >> 65);
        //a+c;
    }
    return 0;
}
