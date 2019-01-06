/*
Copyright (C) 2018  Evariste COURJAUD F5OEO

This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "dsp.h"
#include <stdlib.h>

dsp::dsp()
{
}

dsp::dsp(uint32_t srate):samplerate(srate)
{
}


#define ln(x) (log(x)/log(2.718281828459045235f))

// Again some functions taken gracefully from F4GKR : https://github.com/f4gkr/RadiantBee

//Normalize to [-180,180):
inline double dsp::constrainAngle(double x){
    x = fmod(x + M_PI,2*M_PI);
    if (x < 0)
        x += 2*M_PI;
    return x - M_PI;
}

// convert to [-360,360]
inline double dsp::angleConv(double angle){
    return fmod(constrainAngle(angle),2*M_PI);
}
inline double dsp::angleDiff(double a,double b){
    double dif = fmod(b - a + M_PI,2*M_PI);
    if (dif < 0)
        dif += 2*M_PI;
    return dif - M_PI;
}

inline double dsp::unwrap(double previousAngle,double newAngle){
    return previousAngle - angleDiff(newAngle,angleConv(previousAngle));
}


int dsp::arctan2(int y, int x) // Should be replaced with fast_atan2 from rtl_fm
{
	int abs_y = abs(y);
	int angle;
	if((x==0)&&(y==0)) return 0;
	if(x >= 0){
		angle = 45 - 45 * (x - abs_y) / ((x + abs_y)==0?1:(x + abs_y));
	} else {
		angle = 135 - 45 * (x + abs_y) / ((abs_y - x)==0?1:(abs_y - x));
	}
	return (y < 0) ? -angle : angle; // negate if in quad III or IV
}

void dsp::pushsample(std::complex<float> sample)
{

	amplitude=abs(sample);	

    double phase=atan2(sample.imag(),sample.real());
	//dbg_printf(1,"phase %f\n",phase);
    phase=unwrap(prev_phase,phase);

    double dp= phase-prev_phase;

	frequency = (dp*(double)samplerate)/(2.0*M_PI);
    prev_phase = phase;
}


