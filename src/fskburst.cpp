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
#include "fskburst.h"
#include <unistd.h>

	fskburst::fskburst(uint64_t TuneFrequency,uint32_t SymbolRate,float Deviation,int Channel,uint32_t FifoSize):bufferdma(Channel,FifoSize+3,2,1),freqdeviation(Deviation)
	{
		
		clkgpio::SetAdvancedPllMode(true);
		clkgpio::SetCenterFrequency(TuneFrequency,SymbolRate); // Write Mult Int and Frac : FixMe carrier is already there
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

	fskburst::~fskburst()
	{
	}

	void fskburst::SetDmaAlgo()
{
			sampletab[buffersize*registerbysample-2]=(Originfsel & ~(7 << 12)) | (4 << 12); //Enable Clk
			sampletab[buffersize*registerbysample-1]=(Originfsel & ~(7 << 12)) | (0 << 12); //Disable Clk

			dma_cb_t *cbp = cbarray;
			// We must fill the FIFO (PWM or PCM) to be Synchronized from start
			// PWM FIFO = 16
			// PCM FIFO = 64
			if(syncwithpwm)
			{
				SetEasyCB(cbp++,0,dma_pwm,16+1);
			}
			else
			{
				SetEasyCB(cbp++,0,dma_pcm,64+1);
				
			}

			SetEasyCB(cbp++,buffersize*registerbysample-2,dma_fsel,1);//Enable clk

			
			for (uint32_t samplecnt = 0; samplecnt < buffersize-2; samplecnt++) 
			{ 
			

				// Write a frequency sample
				SetEasyCB(cbp++,samplecnt*registerbysample,dma_pllc_frac,1);//Enable clk
								
				// Delay
				SetEasyCB(cbp++,samplecnt*registerbysample,syncwithpwm?dma_pwm:dma_pcm,1);
			
			}
			lastcbp=cbp;
			
			SetEasyCB(cbp,buffersize*registerbysample-1,dma_fsel,1);//Disable clk
			cbp->next = 0; // Stop DMA			
		
		//dbg_printf(1,"Last cbp :  src %x dest %x next %x\n",cbp->src,cbp->dst,cbp->next);
}
	void fskburst::SetSymbols(unsigned char *Symbols,uint32_t Size)
	{
		if(Size>buffersize-3) {dbg_printf(1,"Buffer overflow\n");return;}
		
		dma_cb_t *cbp=cbarray;
		cbp+=2; // Skip the first 2 CB (initialisation)
		for(unsigned int i=0;i<Size;i++)
		{
			sampletab[i]=(0x5A<<24)|GetMasterFrac((Symbols[i]==0)?-freqdeviation:freqdeviation);
			cbp++;//SKIP FREQ CB
			cbp->next = mem_virt_to_phys(cbp + 1);
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

