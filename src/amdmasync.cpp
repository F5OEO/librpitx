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
#include "amdmasync.h"
#include "gpio.h"
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sched.h>


amdmasync::amdmasync(uint64_t TuneFrequency,uint32_t SR,int Channel,uint32_t FifoSize):bufferdma(Channel,FifoSize,3,2)
{

	SampleRate=SR;
	tunefreq=TuneFrequency;
	clkgpio::SetAdvancedPllMode(true);
	clkgpio::SetCenterFrequency(TuneFrequency,SampleRate); 
	clkgpio::SetFrequency(0);
	clkgpio::enableclk(4); // GPIO 4 CLK by default
	syncwithpwm=false;
	
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
	
	padgpio pad;
	Originfsel=pad.gpioreg[PADS_GPIO_0];
   
	SetDmaAlgo();

		
}

amdmasync::~amdmasync()
{
	clkgpio::disableclk(4);
	padgpio pad;
	pad.gpioreg[PADS_GPIO_0]=Originfsel;
}



void amdmasync::SetDmaAlgo()
{
			dma_cb_t *cbp = cbarray;
			for (uint32_t samplecnt = 0; samplecnt < buffersize; samplecnt++) 
			{ 
			
			//@0				
			//Set Amplitude by writing to PADS	
			cbp->info = 0;//BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
			cbp->src = mem_virt_to_phys(&usermem[samplecnt*registerbysample]);
			cbp->dst = 0x7E000000+(PADS_GPIO_0<<2)+PADS_GPIO;
			cbp->length = 4;
			cbp->stride = 0;
			cbp->next = mem_virt_to_phys(cbp + 1);		
			cbp++;

			//@1				
			//Set Amplitude  to FSEL for amplitude=0
			cbp->info = 0;//BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
			cbp->src = mem_virt_to_phys(&usermem[samplecnt*registerbysample+1]); 
			cbp->dst = 0x7E000000 + (GPFSEL0<<2)+GENERAL_BASE; 				
			cbp->length = 4;
			cbp->stride = 0;
			cbp->next = mem_virt_to_phys(cbp + 1); 
			cbp++;
			
				
			// Delay
			if(syncwithpwm)
				cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP |BCM2708_DMA_D_DREQ  | BCM2708_DMA_PER_MAP(DREQ_PWM);
			else
				cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP |BCM2708_DMA_D_DREQ  | BCM2708_DMA_PER_MAP(DREQ_PCM_TX);
			cbp->src = mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed the PWM
			if(syncwithpwm)		
				cbp->dst = 0x7E000000 + (PWM_FIFO<<2) + PWM_BASE ;
			else
				cbp->dst = 0x7E000000 + (PCM_FIFO_A<<2) + PCM_BASE ;
			cbp->length = 4;
			cbp->stride = 0;
			cbp->next = mem_virt_to_phys(cbp + 1);
			//fprintf(stderr,"cbp : sample %x src %x dest %x next %x\n",samplecnt,cbp->src,cbp->dst,cbp->next);
			cbp++;
			
		}
					
		cbp--;
		cbp->next = mem_virt_to_phys(cbarray); // We loop to the first CB
		//fprintf(stderr,"Last cbp :  src %x dest %x next %x\n",cbp->src,cbp->dst,cbp->next);
}

void amdmasync::SetAmSample(uint32_t Index,float Amplitude) //-1;1
{
	Index=Index%buffersize;	

	int IntAmplitude=round(abs(Amplitude)*8.0)-1;
	
	int IntAmplitudePAD=IntAmplitude;
	if(IntAmplitudePAD>7) IntAmplitudePAD=7;
	if(IntAmplitudePAD<0) IntAmplitudePAD=0;
	
	//fprintf(stderr,"Amplitude=%f PAD %d\n",Amplitude,IntAmplitudePAD);
	sampletab[Index*registerbysample]=(0x5A<<24) + (IntAmplitudePAD&0x7) + (1<<4) + (0<<3); // Amplitude PAD

	//sampletab[Index*registerbysample+2]=(Originfsel & ~(7 << 12)) | (4 << 12); //Alternate is CLK
	if(IntAmplitude==-1)
		{
			sampletab[Index*registerbysample+1]=(Originfsel & ~(7 << 12)) | (0 << 12); //Pin is in -> Amplitude 0
		}
		else
		{
			sampletab[Index*registerbysample+1]=(Originfsel & ~(7 << 12)) | (4 << 12); //Alternate is CLK
		}
	

	PushSample(Index);
}

void amdmasync::SetAmSamples(float *sample,size_t Size)
{
	size_t NbWritten=0;
	int OSGranularity=100;

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
			//fprintf(stderr,"buffer size %d Available %d SampleRate %d Sleep %d\n",buffersize,Available,SampleRate,TimeToSleep);
			usleep(TimeToSleep);
		}
		else
		{
			//fprintf(stderr,"No Sleep %d\n",TimeToSleep);	
			sched_yield();
		}
		clock_gettime(CLOCK_REALTIME, &gettime_now);
		time_difference = gettime_now.tv_nsec - start_time;
		if(time_difference<0) time_difference+=1E9;
		//fprintf(stderr,"Measure samplerate=%d\n",(int)((GetBufferAvailable()-Available)*1e9/time_difference));
		Available=GetBufferAvailable();
		int Index=GetUserMemIndex();
		int ToWrite=((int)Size-(int)NbWritten)<Available?Size-NbWritten:Available;
		
		for(int i=0;i<ToWrite;i++)
		{
			SetAmSample(Index+i,sample[NbWritten++]);
		}
		
	}
}


