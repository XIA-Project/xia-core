#pragma once

// zipf workload generator modified from the code at http://www.nr.com/forum/showthread.php?t=1396

#include <iostream>
#include <cmath>
#include <cstdlib>

//
// Discrete Zipf distribution: 
// p(k) is proportional to (v+k)**(-s) where s > 1, k >= 0.
//
// An implementation based on
//
// W.Hormann, G.Derflinger:
// "Rejection-Inversion to Generate Variates 
// from Monotone Discrete Distributions"
//    http://eeyore.wu-wien.ac.at/papers/96-04-04.wh-der.ps.gz
//
//
// The values generated are in the range of [0, imax]
//
class Zipf {
  public:
	typedef float Doub;

	typedef Doub (*udPoint)();// pointer to function that returns double

    // The first parameter is the Zipf distribution parameter
	//
    // The second parameter defines the upper end of the range
    // of generated values. The default value is the largest possible
    // unsigned integer in the system.
    //
    // The third parameter is a pointer to a function that returns a
    // uniformly destributed random double on [0,1]
    //
    // The forth parameter is the "v" in the distribution
    // function (v+k)**(-s). The default value is 1.
    //
    // The value of s must be greater than 1; v must be greater
    // than or equal to one. If the user supplies invalid values,
    // the setup function informs that they are being changed.
    // On general principles, I just don't like constructors that
    // can fail. On the other hand, I didn't want to make the user
    // call a separate setup function to initialize the object.
    //  Chacun a son gout!
    //
    //
	Zipf(Doub s,
			unsigned long maxi= static_cast<unsigned long>(-1),
			udPoint ran = uDoub,
			int vv = 1
		):
		imax(maxi), v(vv), q(s), urand(ran) {setup();}

    //
    // return a discrete random number from the discrete Zipf
    // distribution in the range of [0, imax]
    //
    unsigned long next() 
    {
        while (true) {
            Doub ur = hxm + urand() * (hx0MinusHxm);
            Doub x = Hinv(ur);

            unsigned long k = static_cast<unsigned long>(x + 0.5);
            if (k - x <= s) {
                return k;
            }
            else if (ur >= (H(k + 0.5) - exp(-log(k + v) * q))) {
                return k;
            }
        }
    }
    Doub get_s() const {return s;}
    Doub get_v() const {return v;}

    //
    // A function that uses nr3 Ran to generate a
    // uniformly distributed double on [0,1]
    // A pointer to this function is used in
    // the constructor for Zipf
    //
    static Doub uDoub()
    {
	//static Ran ran(time(0));
	//return ran.doub();
	return (float)rand() / RAND_MAX;
    }

  private:
    Doub imax;
    Doub v;
    Doub q, oneMinusQ, oneMinusQ_Inverse, hx0MinusHxm, hxm, s;

    udPoint urand;      // pointer to function that returns uniformly 
                        // distributed double on [0,1]


    void setup()
    {
        if (q <= 1.0) {
            std::cout << "Input value for s is " << q
                 << ", which is invalid. changing to 2.0" << std::endl;
            q = 2.0;
        }
        if (v < 1) {
            std::cout << "Input value for v is " << v
                 << ", which is invalid. changing to 1" << std::endl;
            v = 1;
        }

        oneMinusQ         = 1.0 - q;
        oneMinusQ_Inverse = 1.0 / oneMinusQ;
        hxm               = H(imax + 0.5);
        hx0MinusHxm       = H(0.5) - exp(log(v) * (-q)) - hxm;
        s                 = 1 - Hinv(H(1.5) - exp(-q * log(v + 1.0)));

    }

    Doub H(const Doub & x) const
    {
        return (exp(oneMinusQ * log(v + x)) * oneMinusQ_Inverse);
    }

    Doub Hinv(const Doub & x) const
    {
        return exp(oneMinusQ_Inverse * log(oneMinusQ * x)) - v;
    }

};

