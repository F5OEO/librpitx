#ifndef DEF_IQDMASYNC
#define DEF_IQDMASYNC

#include "stdint.h"
#include "dma.h"
#include "gpio.h"
#include "dsp.h"


class iqdmasync:public bufferdma,public clkgpio,public pwmgpio,public pcmgpio
{
	protected:
	uint64_t tunefreq;
	bool syncwithpwm;
	dsp mydsp;
	uint32_t	Originfsel; //Save the original FSEL GPIO
	uint32_t SampleRate;
	public:
	iqdmasync(uint64_t TuneFrequency,uint32_t SR,int Channel,uint32_t FifoSize);
	~iqdmasync();
	void SetDmaAlgo();
	
	void SetPhase(bool inversed);
	void SetIQSample(uint32_t Index,std::complex<float> sample,int Harmonic);
	void SetIQSamples(std::complex<float> *sample,size_t Size,int Harmonic);
}; 

#endif
