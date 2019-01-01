#ifndef DEF_ATV
#define DEF_ATV

#include "stdint.h"
#include "stdio.h"
#include "dma.h"
#include "gpio.h"

class atv:public dma,public clkgpio,public pwmgpio,public pcmgpio
{
	protected:
	uint64_t tunefreq;
	bool syncwithpwm;
	uint32_t Originfsel;
	uint32_t SampleRate;
	public:
	atv(uint64_t TuneFrequency,uint32_t SR,int Channel,uint32_t FifoSize);
	~atv();
	void SetDmaAlgo();

	void SetFrame(unsigned char *Luminance,size_t Lines);	
	//void SetTvSample(uint32_t Index,float Amplitude);
	//void SetTvSamples(float *sample,size_t Size);
}; 

#endif
