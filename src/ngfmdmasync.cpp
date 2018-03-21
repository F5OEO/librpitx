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


ngfmdmasync::ngfmdmasync(uint64_t TuneFrequency,uint32_t SR,int Channel,uint32_t FifoSize):bufferdma(Channel,FifoSize,2,1)
{

	SampleRate=SR;
	tunefreq=TuneFrequency;
	clkgpio::SetAdvancedPllMode(true);
	clkgpio::SetCenterFrequency(TuneFrequency,SampleRate); // Write Mult Int and Frac : FixMe carrier is already there
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

			cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP ;
			cbp->src = mem_virt_to_phys(&usermem[samplecnt*registerbysample]);
			cbp->dst = 0x7E000000 + (PLLA_FRAC<<2) + CLK_BASE ; 
			cbp->length = 4;
			cbp->stride = 0;
			cbp->next = mem_virt_to_phys(cbp + 1);
			//fprintf(stderr,"cbp : sample %x src %x dest %x next %x\n",samplecnt,cbp->src,cbp->dst,cbp->next);
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

void ngfmdmasync::SetFrequencySample(uint32_t Index,double Frequency)
{
	Index=Index%buffersize;	
	sampletab[Index]=(0x5A<<24)|GetMasterFrac(Frequency);
	//fprintf(stderr,"Frac=%d\n",GetMasterFrac(Frequency));
	PushSample(Index);
}

void ngfmdmasync::SetFrequencySamples(double *sample,size_t Size)
{
	size_t NbWritten=0;
	int OSGranularity=100;
	while(NbWritten<Size)
	{
		int Available=GetBufferAvailable();
		int TimeToSleep=1e6*((int)buffersize*3/4-Available-OSGranularity)/SampleRate; // Sleep for theorically fill 3/4 of Fifo
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
		Available=GetBufferAvailable();
		int Index=GetUserMemIndex();
		int ToWrite=((int)Size-(int)NbWritten)<Available?Size-NbWritten:Available;
		
		for(int i=0;i<ToWrite;i++)
		{
			SetFrequencySample(Index+i,sample[NbWritten++]);
		}
		
	}
}


