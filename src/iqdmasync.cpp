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
#include "iqdmasync.h"
#include <unistd.h>
#include <sched.h>


iqdmasync::iqdmasync(uint64_t TuneFrequency,uint32_t SR,int Channel,uint32_t FifoSize):bufferdma(Channel,FifoSize,4,3)
{
// Usermem :
// FRAC frequency
// PAD Amplitude
// FSEL for amplitude 0
	SampleRate=SR;
	tunefreq=TuneFrequency;
	clkgpio::SetAdvancedPllMode(true);
	clkgpio::SetCenterFrequency(TuneFrequency,SampleRate); // Write Mult Int and Frac : FixMe carrier is already there
	clkgpio::SetFrequency(0);
	clkgpio::enableclk(4);
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
	
	mydsp.samplerate=SampleRate;
   
	padgpio pad;
	Originfsel=pad.gpioreg[PADS_GPIO_0];

	SetDmaAlgo();

	
	// Note : Spurious are at +/-(19.2MHZ/2^20)*Div*N : (N=1,2,3...) So we need to have a big div to spurious away BUT
	// Spurious are ALSO at +/-(19.2MHZ/2^20)*(2^20-Div)*N
	// Max spurious avoid is to be in the center ! Theory shoud be that spurious are set away at 19.2/2= 9.6Mhz ! But need to get account of div of PLLClock
	
}

iqdmasync::~iqdmasync()
{
	padgpio pad;
	pad.gpioreg[PADS_GPIO_0]=Originfsel;
	clkgpio::disableclk(4);	
}

void iqdmasync::SetPhase(bool inversed)
{
	clkgpio::SetPhase(inversed);
}

void iqdmasync::SetDmaAlgo()
{
	dma_cb_t *cbp = cbarray;
	for (uint32_t samplecnt = 0; samplecnt < buffersize; samplecnt++) 
		{ 
			
			

			//@0				
			//Set Amplitude by writing to PADS	
			cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
			cbp->src = mem_virt_to_phys(&usermem[samplecnt*registerbysample+1]);
			cbp->dst = 0x7E000000+(PADS_GPIO_0<<2)+PADS_GPIO;
			cbp->length = 4;
			cbp->stride = 0;
			cbp->next = mem_virt_to_phys(cbp + 1);		
			cbp++;

			//@2 Write a frequency sample : Order of DMA CS influence maximum rate : here 0,2,1 is the best : why !!!!!!

			cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP ;
			cbp->src = mem_virt_to_phys(&usermem[samplecnt*registerbysample]);
			cbp->dst = 0x7E000000 +  (PLLA_FRAC<<2) + CLK_BASE ; 
			cbp->length = 4;
			cbp->stride = 0;
			cbp->next = mem_virt_to_phys(cbp + 1);
			//fprintf(stderr,"cbp : sample %x src %x dest %x next %x\n",samplecnt,cbp->src,cbp->dst,cbp->next);
			cbp++;
			
			//@1				
			//Set Amplitude  to FSEL for amplitude=0
			cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
			cbp->src = mem_virt_to_phys(&usermem[samplecnt*registerbysample+2]); 
			cbp->dst = 0x7E000000 + (GPFSEL0<<2)+GENERAL_BASE; 				
			cbp->length = 4;
			cbp->stride = 0;
			cbp->next = mem_virt_to_phys(cbp + 1); 
			cbp++;

			
		
				
			//@3 Delay
			if(syncwithpwm)
				cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP |BCM2708_DMA_D_DREQ  | BCM2708_DMA_PER_MAP(DREQ_PWM);
			else
				cbp->info =BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP |BCM2708_DMA_D_DREQ  | BCM2708_DMA_PER_MAP(DREQ_PCM_TX);
			cbp->src = mem_virt_to_phys(&usermem[(samplecnt+1)*registerbysample]);//mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed the PWM
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


void iqdmasync::SetIQSample(uint32_t Index,std::complex<float> sample,int Harmonic)
{
	Index=Index%buffersize;	
	mydsp.pushsample(sample);
	/*if(mydsp.frequency>2250) mydsp.frequency=2250;
	if(mydsp.frequency<1000) mydsp.frequency=1000;*/
	sampletab[Index*registerbysample]=(0x5A<<24)|GetMasterFrac(mydsp.frequency/Harmonic); //Frequency
	int IntAmplitude=(int)(mydsp.amplitude*8.0)-1; //Fixme 1e4 seems to work with SSB but should be an issue with classical IQ file 

	int IntAmplitudePAD=0;
	if(IntAmplitude>7) IntAmplitudePAD=7;
	if(IntAmplitude<0) IntAmplitudePAD=0;
	sampletab[Index*registerbysample+1]=(0x5A<<24) + (IntAmplitudePAD&0x7) + (1<<4) + (0<<3); // Amplitude PAD

	//sampletab[Index*registerbysample+2]=(Originfsel & ~(7 << 12)) | (4 << 12); //Alternate is CLK
	if(IntAmplitude==-1)
		{
			sampletab[Index*registerbysample+2]=(Originfsel & ~(7 << 12)) | (0 << 12); //Pin is in -> Amplitude 0
		}
		else
		{
			sampletab[Index*registerbysample+2]=(Originfsel & ~(7 << 12)) | (4 << 12); //Alternate is CLK : Fixme : do not work with clk2
		}
	
		//fprintf(stderr,"amp%f %d\n",mydsp.amplitude,IntAmplitudePAD);
	PushSample(Index);
}

void iqdmasync::SetIQSamples(std::complex<float> *sample,size_t Size,int Harmonic=1)
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
			//fprintf(stderr,"buffer size %d Available %d SampleRate %d Sleep %d\n",buffersize,Available,SampleRate,TimeToSleep);
			usleep(TimeToSleep);
		}
		else
		{
			fprintf(stderr,"No Sleep %d\n",TimeToSleep);	
			//sched_yield();
		}

		if(debug>0)
		{	
		clock_gettime(CLOCK_REALTIME, &gettime_now);
		time_difference = gettime_now.tv_nsec - start_time;
		if(time_difference<0) time_difference+=1E9;
		//fprintf(stderr,"Available %d Measure samplerate=%d\n",GetBufferAvailable(),(int)((GetBufferAvailable()-Available)*1e9/time_difference));
		debug--;	
		}
		Available=GetBufferAvailable();
		
		int Index=GetUserMemIndex();
		int ToWrite=((int)Size-(int)NbWritten)<Available?Size-NbWritten:Available;
		//printf("Available after=%d Timetosleep %d To Write %d\n",Available,TimeToSleep,ToWrite);		
		for(int i=0;i<ToWrite;i++)
		{
			
			SetIQSample(Index+i,sample[NbWritten++],Harmonic);
		}
		
	}
}



