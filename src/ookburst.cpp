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
#include "util.h"

ookburst::ookburst(uint64_t TuneFrequency, float SymbolRate, int Channel, uint32_t FifoSize, size_t upsample, float RatioRamp) : bufferdma(Channel, FifoSize * upsample + 2, 2, 1),SR_upsample(upsample)
{

	clkgpio::SetAdvancedPllMode(true);

	clkgpio::SetCenterFrequency(TuneFrequency, 0); // Bandwidth is 0 because frequency always the same
	clkgpio::SetFrequency(0);

	syncwithpwm = false;
	Ramp = SR_upsample * RatioRamp; //Ramp time

	if (syncwithpwm)
	{
		pwmgpio::SetPllNumber(clk_plld, 1);
		pwmgpio::SetFrequency(SymbolRate * (float)upsample);
	}
	else
	{
		pcmgpio::SetPllNumber(clk_plld, 1);
		pcmgpio::SetFrequency(SymbolRate * float(upsample));
	}

	Originfsel = clkgpio::gengpio.gpioreg[GPFSEL0];
	SetDmaAlgo();
}

ookburst::~ookburst()
{
}

void ookburst::SetDmaAlgo()
{
	dma_cb_t *cbp = cbarray;
	// We must fill the FIFO (PWM or PCM) to be Synchronized from start
	// PWM FIFO = 16
	// PCM FIFO = 64
	if (syncwithpwm)
	{
		SetEasyCB(cbp++, 0, dma_pwm, 16 + 1);
	}
	else
	{
		SetEasyCB(cbp++, 0, dma_pcm, 64 + 1);
	}

	for (uint32_t samplecnt = 0; samplecnt < buffersize - 2; samplecnt++)
	{

		//Set Amplitude  to FSEL for amplitude=0
		SetEasyCB(cbp++, samplecnt * registerbysample, dma_fsel, 1);
		// Delay
		SetEasyCB(cbp++, samplecnt * registerbysample, syncwithpwm ? dma_pwm : dma_pcm, 1);
	}
	lastcbp = cbp;

	// Last CBP before stopping : disable output
	sampletab[buffersize * registerbysample - 1] = (Originfsel & ~(7 << 12)) | (0 << 12); //Disable Clk
	SetEasyCB(cbp, buffersize * registerbysample - 1, dma_fsel, 1);
	cbp->next = 0; // Stop DMA
}
void ookburst::SetSymbols(unsigned char *Symbols, uint32_t Size)
{
	if (Size > buffersize - 2)
	{
		dbg_printf(1, "Buffer overflow\n");
		return;
	}

	dma_cb_t *cbp = cbarray;
	cbp++; // Skip the first which is the Fiiling of Fifo

	for (unsigned i = 0; i < Size; i++)
	{
		for (size_t j = 0; j < SR_upsample - Ramp; j++)
		{
			sampletab[i * SR_upsample + j] = (Symbols[i] == 0) ? ((Originfsel & ~(7 << 12)) | (0 << 12)) : ((Originfsel & ~(7 << 12)) | (4 << 12));
			cbp++; //SKIP FSEL CB
			cbp->next = mem_virt_to_phys(cbp + 1);
			cbp++;
		}
		for (size_t j = 0; j < Ramp; j++)
		{
			if (i < Size - 1)
			{
				sampletab[i * SR_upsample + j + SR_upsample - Ramp] = (Symbols[i] == 0) ? ((Originfsel & ~(7 << 12)) | (0 << 12)) : ((Originfsel & ~(7 << 12)) | (4 << 12));
				
			}
			else
			{
				sampletab[i * SR_upsample + j + SR_upsample - Ramp] = (Symbols[i] == 0) ? ((Originfsel & ~(7 << 12)) | (0 << 12)) : ((Originfsel & ~(7 << 12)) | (4 << 12));
			}

			cbp++; //SKIP FREQ CB
			cbp->next = mem_virt_to_phys(cbp + 1);
			cbp++;
		}
	}

	cbp--;
	cbp->next = mem_virt_to_phys(lastcbp);

	dma::start();

	while (isrunning()) //Block function : return until sent completely signal
	{
		usleep(100);
	}
	usleep(100); //To be sure last symbol Tx ?
}

//****************************** OOK BURST TIMING *****************************************
// SampleRate is set to 0.1MHZ,means 10us granularity, MaxMessageDuration in us
ookbursttiming::ookbursttiming(uint64_t TuneFrequency, size_t MaxMessageDuration) : ookburst(TuneFrequency, 1e5, 14, MaxMessageDuration / 10)
{
	m_MaxMessage = MaxMessageDuration;
	ookrenderbuffer = new unsigned char[m_MaxMessage];
}

ookbursttiming::~ookbursttiming()
{
	if (ookrenderbuffer != nullptr)
		delete[] ookrenderbuffer;
}

void ookbursttiming::SendMessage(SampleOOKTiming *TabSymbols, size_t Size)
{
	size_t n = 0;
	for (size_t i = 0; i < Size; i++)
	{
		for (size_t j = 0; j < TabSymbols[i].duration / 10; j++)
		{
			ookrenderbuffer[n++] = TabSymbols[i].value;
			if (n >= m_MaxMessage)
			{
				dbg_printf(1, "OOK Message too long abort time(%d/%d)\n", n, m_MaxMessage);
				return;
			}
		}
	}
	SetSymbols(ookrenderbuffer, n);
}
