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


#include "stdio.h"
#include "serialdmasync.h"


serialdmasync::serialdmasync(uint32_t SampleRate,int Channel,uint32_t FifoSize,bool dualoutput):bufferdma(Channel,FifoSize,1,1)
{
	if(dualoutput) //Fixme if 2pin we want maybe 2*SRATE as it is distributed over 2 pin
	{
		pwmgpio::SetMode(pwm2pin);
		SampleRate*=2;
	}
	else
	{
		pwmgpio::SetMode(pwm1pin);
	}

	if(SampleRate>250000)
	{
		pwmgpio::SetPllNumber(clk_plld,1);
		pwmgpio::SetFrequency(SampleRate);
	}
	else
	{
		pwmgpio::SetPllNumber(clk_osc,1);
		pwmgpio::SetFrequency(SampleRate);
	}

	enablepwm(12,0); // By default PWM on GPIO 12/pin 32	
    enablepwm(13,0); // By default PWM on GPIO 13/pin 33	

	SetDmaAlgo();

		
}

serialdmasync::~serialdmasync()
{
}


void serialdmasync::SetDmaAlgo()
{
			dma_cb_t *cbp = cbarray;
			for (uint32_t samplecnt = 0; samplecnt < buffersize; samplecnt++) 
			{ 
			
				SetEasyCB(cbp,samplecnt*registerbysample,dma_pwm,1);
				cbp++;
			
			}
					
		cbp--;
		cbp->next = mem_virt_to_phys(cbarray); // We loop to the first CB
		//dbg_printf(1,"Last cbp :  src %x dest %x next %x\n",cbp->src,cbp->dst,cbp->next);
}

void serialdmasync::SetSample(uint32_t Index,int Sample)
{
	Index=Index%buffersize;	
	sampletab[Index]=Sample;
	
	PushSample(Index);
}


