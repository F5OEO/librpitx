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
#include <unistd.h>
#include "ookburst.h"


	ookburst::ookburst(uint64_t TuneFrequency,uint32_t SymbolRate,int Channel,uint32_t FifoSize):bufferdma(Channel,FifoSize+2,2,1)
	{
		
		clkgpio::SetAdvancedPllMode(true);
		
		clkgpio::SetCenterFrequency(TuneFrequency,0); // Bandwidth is 0 because frequency always the same
		clkgpio::SetFrequency(0);
		
		syncwithpwm=false;
	
		if(syncwithpwm)
		{
			pwmgpio::SetPllNumber(clk_plld,1);
			pwmgpio::SetFrequency(SymbolRate);
		}
		else
		{
			pcmgpio::SetPllNumber(clk_plld,1);
			pcmgpio::SetFrequency(SymbolRate);
		}
	
	
	   
		SetDmaAlgo();

		padgpio pad;
		Originfsel=pad.gpioreg[PADS_GPIO_0];
	}

	ookburst::~ookburst()
	{
	}

	void ookburst::SetDmaAlgo()
{
			dma_cb_t *cbp=cbarray;
			// We must fill the FIFO (PWM or PCM) to be Synchronized from start
			// PWM FIFO = 16
			// PCM FIFO = 64
			if(syncwithpwm)
			{
				cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP |BCM2708_DMA_D_DREQ  | BCM2708_DMA_PER_MAP(DREQ_PWM);
				cbp->src = mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed the PWM
				cbp->dst = 0x7E000000 + (PWM_FIFO<<2) + PWM_BASE ;
				cbp->length = 4*(16+1);
				cbp->stride = 0;
				cbp->next = mem_virt_to_phys(cbp + 1);
				cbp++;
			}
			else
			{
				cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP |BCM2708_DMA_D_DREQ  | BCM2708_DMA_PER_MAP(DREQ_PCM_TX);
				cbp->src = mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed PCM
				cbp->dst = 0x7E000000 + (PCM_FIFO_A<<2) + PCM_BASE ;
				cbp->length = 4*(64+1);
				cbp->stride = 0;
				cbp->next = mem_virt_to_phys(cbp + 1);
				//fprintf(stderr,"cbp : sample %x src %x dest %x next %x\n",samplecnt,cbp->src,cbp->dst,cbp->next);
				cbp++;
			}
			
			for (uint32_t samplecnt = 0; samplecnt < buffersize-2; samplecnt++) 
			{ 
			
								
				//Set Amplitude  to FSEL for amplitude=0
				cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
				cbp->src = mem_virt_to_phys(&usermem[samplecnt*registerbysample]); 
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
				//fprintf(stderr,"cbp : sample %d pointer %p src %x dest %x next %x\n",samplecnt,cbp,cbp->src,cbp->dst,cbp->next);
				cbp++;
			
			}
			lastcbp=cbp;

			// Last CBP before stopping : disable output
			sampletab[buffersize*registerbysample-1]=(Originfsel & ~(7 << 12)) | (0 << 12); //Disable Clk
			cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
			cbp->src = mem_virt_to_phys(&usermem[(buffersize*registerbysample-1)]); 
			cbp->dst = 0x7E000000 + (GPFSEL0<<2)+GENERAL_BASE; 				
			cbp->length = 4;
			cbp->stride = 0;
			cbp->next = 0; // Stop DMA			
		
		//fprintf(stderr,"Last cbp %p:  src %x dest %x next %x\n",cbp,cbp->src,cbp->dst,cbp->next);
}
	void ookburst::SetSymbols(unsigned char *Symbols,uint32_t Size)
	{
		if(Size>buffersize-2) {fprintf(stderr,"Buffer overflow\n");return;}

		dma_cb_t *cbp=cbarray;
		cbp++; // Skip the first which is the Fiiling of Fifo
		
		
		for(unsigned i=0;i<Size;i++)
		{
			
			sampletab[i]=(Symbols[i]==0)?((Originfsel & ~(7 << 12)) | (0 << 12)):((Originfsel & ~(7 << 12)) | (4 << 12));
		
			cbp++;//SKIP FSEL CB
			cbp->next = mem_virt_to_phys(cbp + 1);
			
			//fprintf(stderr,"cbp : sample %d pointer %p src %x dest %x next %x\n",i,cbp,cbp->src,cbp->dst,cbp->next);
			cbp++;	
			
		}
		
		cbp--;
		cbp->next = mem_virt_to_phys(lastcbp);
		
				
		dma::start();
		
		
		while(isrunning()) //Block function : return until sent completely signal
		{
			usleep(100);
			
		}
		usleep(100);//To be sure last symbol Tx ?
			
	}

	//****************************** OOK BURST TIMING *****************************************
	// SampleRate is set to 0.1MHZ,means 10us granularity, MaxMessageDuration in us
	ookbursttiming::ookbursttiming(uint64_t TuneFrequency,size_t MaxMessageDuration):ookburst(TuneFrequency,1e5,14,MaxMessageDuration/10)
	{
		m_MaxMessage=MaxMessageDuration;
		ookrenderbuffer=new unsigned char[m_MaxMessage];
	}

	ookbursttiming::~ookbursttiming()
	{
		if(ookrenderbuffer!=nullptr)
			delete []ookrenderbuffer;
	}

	void ookbursttiming::SendMessage(SampleOOKTiming *TabSymbols,size_t Size)
	{
		size_t n=0;
		for (size_t i=0;i<Size;i++)
		{
			for(size_t j=0;j<TabSymbols[i].duration/10;j++)
			{
				ookrenderbuffer[n++]=TabSymbols[i].value;
				if(n>=m_MaxMessage) 
				{
					fprintf(stderr,"OOK Message too long abort time(%d/%d)\n",n,m_MaxMessage);
					return;
				}
			}	
		}
		SetSymbols(ookrenderbuffer,n);
	}
