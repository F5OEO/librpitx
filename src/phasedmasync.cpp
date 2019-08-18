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
#include "phasedmasync.h"
#include <unistd.h>
#include <time.h>
#include "util.h"


//Stable tune for this pwm mode is up to 90MHZ

phasedmasync::phasedmasync(uint64_t TuneFrequency,uint32_t SampleRateIn,int NumberOfPhase,int Channel,uint32_t FifoSize):bufferdma(Channel,FifoSize,2,1) // Number of phase between 2 and 16
{
	SampleRate=SampleRateIn;
	SetMode(pwm1pinrepeat);
	pwmgpio::SetPllNumber(clk_pllc,0);

	tunefreq=TuneFrequency*NumberOfPhase;
	#define MAX_PWM_RATE 360000000
	if(tunefreq>MAX_PWM_RATE) dbg_printf(1,"Critical error : Frequency to high > %d\n",MAX_PWM_RATE/NumberOfPhase);
	if((NumberOfPhase==2)||(NumberOfPhase==4)||(NumberOfPhase==8)||(NumberOfPhase==16)||(NumberOfPhase==32))
		NumbPhase=NumberOfPhase;
	else
		dbg_printf(1,"PWM critical error: %d is not a legal number of phase\n",NumberOfPhase);
	clkgpio::SetAdvancedPllMode(true);
	
	clkgpio::ComputeBestLO(tunefreq,0); // compute PWM divider according to MasterPLL clkgpio::PllFixDivider
	double FloatMult=((double)(tunefreq)*clkgpio::PllFixDivider)/(double)(pwmgpio::XOSC_FREQUENCY);
	uint32_t freqctl = FloatMult*((double)(1<<20)) ; 
	int IntMultiply= freqctl>>20; // Need to be calculated to have a center frequency
	freqctl&=0xFFFFF; // Fractionnal is 20bits
	uint32_t FracMultiply=freqctl&0xFFFFF; 
	clkgpio::SetMasterMultFrac(IntMultiply,FracMultiply);
	dbg_printf(1,"PWM Mult %d Frac %d Div %d\n",IntMultiply,FracMultiply,clkgpio::PllFixDivider);
	
	
	pwmgpio::clk.gpioreg[PWMCLK_DIV] = 0x5A000000 | ((clkgpio::PllFixDivider)<<12) |pwmgpio::pllnumber; // PWM clock input divider				
	usleep(100);
	
	pwmgpio::clk.gpioreg[PWMCLK_CNTL]= 0x5A000000 | (pwmgpio::Mash << 9) | ((clkgpio::PllFixDivider)<<12)| pwmgpio::pllnumber|(1 << 4)  ; //4 is START CLK
	usleep(100);
	pwmgpio::SetPrediv(NumberOfPhase);	//Originaly 32 but To minimize jitter , we set minimal buffer to repeat 
	
	

	enablepwm(12,0); // By default PWM on GPIO 12/pin 32	
	

	pcmgpio::SetPllNumber(clk_plld,1);// Clk for Samplerate by PCM
	pcmgpio::SetFrequency(SampleRate);
	
		
   
	SetDmaAlgo();
	
	uint32_t ZeroPhase=0;
	switch(NumbPhase)
	{
		case 2:ZeroPhase=0xAAAAAAAA;break;//1,0,1,0 1,0,1,0 
		case 4:ZeroPhase=0xCCCCCCCC;break;//1,1,0,0 //4
		case 8:ZeroPhase=0xF0F0F0F0;break;//1,1,1,1,0,0,0,0 //8 
		case 16:ZeroPhase=0xFF00FF00;break;//1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0 //16
		case 32:ZeroPhase=0xFFFF0000;break;//1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 //32
		default:dbg_printf(1,"Zero phase not initialized\n");break;
	}
	
	for(int i=0;i<NumbPhase;i++)	
	{
		TabPhase[i]=ZeroPhase;
		//dbg_printf(1,"Phase[%d]=%x\n",i,TabPhase[i]);
		ZeroPhase=(ZeroPhase<<1)|(ZeroPhase>>31);
	}

	
}

phasedmasync::~phasedmasync()
{
	disablepwm(12);
}


void phasedmasync::SetDmaAlgo()
{
			dma_cb_t *cbp = cbarray;
			for (uint32_t samplecnt = 0; samplecnt < buffersize; samplecnt++) 
			{ 
			
							
			cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP ;
			cbp->src = mem_virt_to_phys(&usermem[samplecnt*registerbysample]); 
			cbp->dst = 0x7E000000 + (PWM_FIFO<<2) + PWM_BASE ;
			cbp->length = 4;
			cbp->stride = 0;
			cbp->next = mem_virt_to_phys(cbp + 1);
			//dbg_printf(1,"cbp : sample %x src %x dest %x next %x\n",samplecnt,cbp->src,cbp->dst,cbp->next);
			cbp++;
			
			
			cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP |BCM2708_DMA_D_DREQ  | BCM2708_DMA_PER_MAP(DREQ_PCM_TX);
			cbp->src = mem_virt_to_phys(&usermem[(samplecnt+1)*registerbysample]);//mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed the PWM
			cbp->dst = 0x7E000000 + (PCM_FIFO_A<<2) + PCM_BASE ;
			cbp->length = 4;
			cbp->stride = 0;
			cbp->next = mem_virt_to_phys(cbp + 1);
			//dbg_printf(1,"cbp : sample %x src %x dest %x next %x\n",samplecnt,cbp->src,cbp->dst,cbp->next);
			cbp++;
		}
					
		cbp--;
		cbp->next = mem_virt_to_phys(cbarray); // We loop to the first CB
		//dbg_printf(1,"Last cbp :  src %x dest %x next %x\n",cbp->src,cbp->dst,cbp->next);
}

void phasedmasync::SetPhase(uint32_t Index,int Phase)
{
	Index=Index%buffersize;
	Phase=(Phase+NumbPhase)%NumbPhase;
	sampletab[Index]=TabPhase[Phase];
	PushSample(Index);
	
}


void phasedmasync::SetPhaseSamples(int *sample,size_t Size)
{
	size_t NbWritten=0;
	int OSGranularity=100;
	
	
	long int start_time;
	long time_difference=0;
	struct timespec gettime_now;


	int debug=1;		
	
	while(NbWritten<Size)
	{
		if(debug>0)
		{	
		clock_gettime(CLOCK_REALTIME, &gettime_now);
		start_time = gettime_now.tv_nsec;	
		}
		int Available=GetBufferAvailable();
		//printf("Available before=%d\n",Available);		
		int TimeToSleep=1e6*((int)buffersize*3/4-Available)/(float)SampleRate/*-OSGranularity*/; // Sleep for theorically fill 3/4 of Fifo
		if(TimeToSleep>0)
		{
			//dbg_printf(1,"buffer size %d Available %d SampleRate %d Sleep %d\n",buffersize,Available,SampleRate,TimeToSleep);
			usleep(TimeToSleep);
		}
		else
		{
			//dbg_printf(1,"No Sleep %d\n",TimeToSleep);	
			//sched_yield();
		}

		if(debug>0)
		{	
		clock_gettime(CLOCK_REALTIME, &gettime_now);
		time_difference = gettime_now.tv_nsec - start_time;
		if(time_difference<0) time_difference+=1E9;
		//dbg_printf(1,"Available %d Measure samplerate=%d\n",GetBufferAvailable(),(int)((GetBufferAvailable()-Available)*1e9/time_difference));
		debug--;	
		}
		Available=GetBufferAvailable();
		
		int Index=GetUserMemIndex();
		int ToWrite=((int)Size-(int)NbWritten)<Available?Size-NbWritten:Available;
		//printf("Available after=%d Timetosleep %d To Write %d\n",Available,TimeToSleep,ToWrite);		
		for(int i=0;i<ToWrite;i++)
		{
			
			SetPhase(Index+i,sample[NbWritten++]);
		}
		
	}
}


