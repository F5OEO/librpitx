#ifndef DEF_OOKBURST
#define DEF_OOKBURST

#include "stdint.h"
#include "dma.h"
#include "gpio.h"

class ookburst:public bufferdma,public clkgpio,public pwmgpio,public pcmgpio
{
	protected:
	float timegranularity; //ns
	uint32_t Originfsel;
	bool syncwithpwm;
	dma_cb_t *lastcbp;
	public:
	ookburst(uint64_t TuneFrequency,uint32_t SymbolRate,int Channel,uint32_t FifoSize);
	~ookburst();
	void SetDmaAlgo();
	
	void SetSymbols(unsigned char *Symbols,uint32_t Size);
}; 

#endif
