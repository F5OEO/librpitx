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
#include "ngfmdmasync.h"
#include <unistd.h>
#include <sched.h>
#include <time.h>

ngfmdmasync::ngfmdmasync(uint64_t TuneFrequency,uint32_t SR,int Channel,uint32_t FifoSize,bool UsePwm):bufferdma(Channel,FifoSize,2,1)
{

	SampleRate=SR;
	tunefreq=TuneFrequency;
	clkgpio::SetAdvancedPllMode(true);
	clkgpio::SetCenterFrequency(TuneFrequency,SampleRate); // Write Mult Int and Frac : FixMe carrier is already there
	clkgpio::SetFrequency(0);
	clkgpio::enableclk(4); // GPIO 4 CLK by default
	syncwithpwm=UsePwm;
	
	if(syncwithpwm)
	{
		pwmgpio::SetPllNumber(clk_plld,1);
		pwmgpio::SetFrequency(SampleRate);
	}
	else
	{
		pcmgpio::SetPllNumber(clk_plld,1);
		pcmgpio::SetFrequency(SampleRate);
	}
	
	
   
	SetDmaAlgo();

	
	// Note : Spurious are at +/-(19.2MHZ/2^20)*Div*N : (N=1,2,3...) So we need to have a big div to spurious away BUT
	// Spurious are ALSO at +/-(19.2MHZ/2^20)*(2^20-Div)*N
	// Max spurious avoid is to be in the center ! Theory shoud be that spurious are set away at 19.2/2= 9.6Mhz ! But need to get account of div of PLLClock
	
}

ngfmdmasync::~ngfmdmasync()
{
	clkgpio::disableclk(4);
}

void ngfmdmasync::SetPhase(bool inversed)
{
	clkgpio::SetPhase(inversed);
}

void ngfmdmasync::SetDmaAlgo()
{
			dma_cb_t *cbp = cbarray;
			for (uint32_t samplecnt = 0; samplecnt < buffersize; samplecnt++) 
			{ 
			
			
			// Write a frequency sample
			SetEasyCB(cbp,samplecnt*registerbysample,dma_pllc_frac,1);
			cbp++;
			
			
			// Delay
			SetEasyCB(cbp,samplecnt*registerbysample,syncwithpwm?dma_pwm:dma_pcm,1);
			cbp++;
			
		}
					
		cbp--;
		cbp->next = mem_virt_to_phys(cbarray); // We loop to the first CB
		//dbg_printf(1,"Last cbp :  src %x dest %x next %x\n",cbp->src,cbp->dst,cbp->next);
}

void ngfmdmasync::SetFrequencySample(uint32_t Index,float Frequency)
{
	Index=Index%buffersize;	
	sampletab[Index]=(0x5A<<24)|GetMasterFrac(Frequency);
	//dbg_printf(1,"Frac=%d\n",GetMasterFrac(Frequency));
	PushSample(Index);
}

void ngfmdmasync::SetFrequencySamples(float *sample,size_t Size)
{
	size_t NbWritten=0;
	int OSGranularity=200;

	long int start_time;
	long time_difference=0;
	struct timespec gettime_now;

	while(NbWritten<Size)
	{
		clock_gettime(CLOCK_REALTIME, &gettime_now);
		start_time = gettime_now.tv_nsec;	
		int Available=GetBufferAvailable();
		int TimeToSleep=1e6*((int)buffersize*3/4-Available)/SampleRate-OSGranularity; // Sleep for theorically fill 3/4 of Fifo
		if(TimeToSleep>0)
		{
			dbg_printf(1,"buffer size %d Available %d SampleRate %d Sleep %d\n",buffersize,Available,SampleRate,TimeToSleep);
			usleep(TimeToSleep);
		}
		else
		{
			dbg_printf(1,"No Sleep %d\n",TimeToSleep);	
			sched_yield();
		}
		clock_gettime(CLOCK_REALTIME, &gettime_now);
		time_difference = gettime_now.tv_nsec - start_time;
		if(time_difference<0) time_difference+=1E9;
		int NewAvailable=GetBufferAvailable();
		dbg_printf(1,"Newavailable %d Measure samplerate=%d\n",NewAvailable,(int)((GetBufferAvailable()-Available)*1e9/time_difference));
		Available=NewAvailable;
		int Index=GetUserMemIndex();
		int ToWrite=((int)Size-(int)NbWritten)<Available?Size-NbWritten:Available;
		
		for(int i=0;i<ToWrite;i++)
		{
			SetFrequencySample(Index+i,sample[NbWritten++]);
		}
		
	}
}


