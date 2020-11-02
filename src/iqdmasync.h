#ifndef DEF_IQDMASYNC
#define DEF_IQDMASYNC

#include "stdint.h"
#include "dma.h"
#include "gpio.h"
#include "dsp.h"

#define MODE_IQ 0
#define MODE_FREQ_A 1

class iqdmasync:public bufferdma,public clkgpio,public pwmgpio,public pcmgpio
{
      
	protected:
	uint64_t tunefreq;
	bool syncwithpwm;
	dsp mydsp;
	uint32_t	Originfsel; //Save the original FSEL GPIO
	uint32_t SampleRate;
	public:
	int ModeIQ=MODE_IQ;
	iqdmasync(uint64_t TuneFrequency,uint32_t SR,int Channel,uint32_t FifoSize,int Mode);
	~iqdmasync();
	void SetDmaAlgo();
	
	void SetPhase(bool inversed);
	void SetIQSample(uint32_t Index,std::complex<float> sample,int Harmonic);
	void SetFreqAmplitudeSample(uint32_t Index,std::complex<float> sample,int Harmonic);
	void SetIQSamples(std::complex<float> *sample,size_t Size,int Harmonic);
}; 

#endif
