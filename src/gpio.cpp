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

extern "C" {
#include "mailbox.h"
}
#include "gpio.h"
#include "raspberry_pi_revision.h"
#include "stdio.h"
#include <unistd.h>
#include <sys/timex.h>
#include <math.h>
#include <string.h>
#include "util.h"
#include "rpi.h"

gpio::gpio(uint32_t base, uint32_t len)
{

	
	gpioreg = (uint32_t *)mapmem(GetPeripheralBase()+base, len);
	gpiolen=len;	
}

gpio::~gpio()
{
	if(gpioreg!=NULL)
		unmapmem((void*)gpioreg,gpiolen);
	
}

uint32_t get_hwbase(void)
{
    const char *ranges_file = "/proc/device-tree/soc/ranges";
    uint8_t ranges[12];
    FILE *fd;
    uint32_t ret = 0;

    memset(ranges, 0, sizeof(ranges));

    if ((fd = fopen(ranges_file, "rb")) == NULL)
    {
        printf("Can't open '%s'\n", ranges_file);
    }
    else if (fread(ranges, 1, sizeof(ranges), fd) >= 8)
    {
        ret = (ranges[4] << 24) |
              (ranges[5] << 16) |
              (ranges[6] << 8) |
              (ranges[7] << 0);
        if (!ret)
            ret = (ranges[8] << 24) |
                  (ranges[9] << 16) |
                  (ranges[10] << 8) |
                  (ranges[11] << 0);
        if ((ranges[0] != 0x7e) ||
                (ranges[1] != 0x00) ||
                (ranges[2] != 0x00) ||
                (ranges[3] != 0x00) ||
                ((ret != 0x20000000) && (ret != 0x3f000000) && (ret != 0xfe000000)))
        {
            printf("Unexpected ranges data (%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x)\n",
                   ranges[0], ranges[1], ranges[2], ranges[3],
                   ranges[4], ranges[5], ranges[6], ranges[7],
                   ranges[8], ranges[9], ranges[10], ranges[11]);
            ret = 0;
        }
    }
    else
    {
	printf("Ranges data too short\n");
    }

    fclose(fd);

    return ret;
}

uint32_t gpio::GetPeripheralBase()
{
	RASPBERRY_PI_INFO_T info;
	uint32_t BCM2708_PERI_BASE =bcm_host_get_peripheral_address();
	dbg_printf(0,"Peri Base = %x SDRAM %x\n",/*get_hwbase()*/bcm_host_get_peripheral_address(),bcm_host_get_sdram_address());
	if(BCM2708_PERI_BASE==0xFE000000) // Fixme , could be inspect without this hardcoded value
	{
               dbg_printf(0,"RPi4 GPIO detected\n");
		pi_is_2711=true;  //Rpi4
		XOSC_FREQUENCY=54000000;
	}
	if(BCM2708_PERI_BASE==0)
	{
		dbg_printf(0,"Unknown peripheral base, switch to PI4 \n");
		BCM2708_PERI_BASE=0xfe000000;
		XOSC_FREQUENCY=54000000;
		pi_is_2711=true;  
	}
	if(pi_is_2711)
		dbg_printf(1,"Running on Pi4\n");
	return BCM2708_PERI_BASE;
}

//******************** DMA Registers ***************************************

dmagpio::dmagpio() : gpio( DMA_BASE, DMA_LEN)
{
}

// ***************** CLK Registers *****************************************
clkgpio::clkgpio() : gpio(CLK_BASE, CLK_LEN)
{
	SetppmFromNTP();
	padgpio level;
	level.setlevel(7); //MAX Power 
}

clkgpio::~clkgpio()
{
	gpioreg[GPCLK_CNTL] = 0x5A000000 | (Mash << 9) | pllnumber | (0 << 4); //4 is START CLK
	//gpioreg[GPCLK_CNTL_2] = 0x5A000000 | (Mash << 9) | pllnumber | (0 << 4); //4 is START CLK
	usleep(100);
}

int clkgpio::SetPllNumber(int PllNo, int MashType)
{
	print_clock_tree();
	if (PllNo < 8)
		pllnumber = PllNo;
	else
		pllnumber = clk_pllc;

	if (MashType < 4)
		Mash = MashType;
	else
		Mash = 0;
	gpioreg[GPCLK_CNTL] = 0x5A000000 | (Mash << 9) | pllnumber /*|(1 << 5)*/; //5 is Reset CLK
	usleep(100);
	//gpioreg[GPCLK_CNTL_2] = 0x5A000000 | (Mash << 9) | pllnumber /*|(1 << 5)*/; //5 is Reset CLK
	//usleep(100);
	Pllfrequency = GetPllFrequency(pllnumber);
	return 0;
}

uint64_t clkgpio::GetPllFrequency(int PllNo)
{
	uint64_t Freq = 0;
	SetppmFromNTP();
	switch (PllNo)
	{
	case clk_osc:
		Freq = XOSC_FREQUENCY;
		break;
	case clk_plla:
		Freq =  (XOSC_FREQUENCY * ((uint64_t)gpioreg[PLLA_CTRL] & 0x3ff) + XOSC_FREQUENCY * (uint64_t)gpioreg[PLLA_FRAC] / (1 << 20))/(2*(gpioreg[PLLA_CTRL] >> 12)&0x7);
		break;
	//case clk_pllb:Freq=XOSC_FREQUENCY*((uint64_t)gpioreg[PLLB_CTRL]&0x3ff) +XOSC_FREQUENCY*(uint64_t)gpioreg[PLLB_FRAC]/(1<<20);break;
	case clk_pllc:
		//Freq = (XOSC_FREQUENCY * ((uint64_t)gpioreg[PLLC_CTRL] & 0x3ff) + (XOSC_FREQUENCY * (uint64_t)gpioreg[PLLC_FRAC]) / (1 << 20)) / (2*gpioreg[PLLC_PER] >> 1)) /((gpioreg[PLLC_CTRL] >> 12)&0x7) ;
		//dbg_printf(1, "Gpio reg %x %x\n",gpioreg[PLLC_PER],gpioreg[PLLC_CTRL] >> 12);
		if((gpioreg[PLLC_PER]!=0)&&(gpioreg[PLLC_CTRL] >> 12 != 0)) 
			Freq =( (XOSC_FREQUENCY * ((uint64_t)gpioreg[PLLC_CTRL] & 0x3ff) + (XOSC_FREQUENCY * (uint64_t)gpioreg[PLLC_FRAC]) / (1 << 20)) / (2*gpioreg[PLLC_PER] >> 1))/((gpioreg[PLLC_CTRL] >> 12)&0x7) ;
		break;
	case clk_plld:
		Freq =( (XOSC_FREQUENCY * ((uint64_t)gpioreg[PLLD_CTRL] & 0x3ff) + (XOSC_FREQUENCY * (uint64_t)gpioreg[PLLD_FRAC]) / (1 << 20)) / (2*gpioreg[PLLD_PER] >> 1))/((gpioreg[PLLD_CTRL] >> 12)&0x7) ;
		break;
	case clk_hdmi:
		Freq =( XOSC_FREQUENCY * ((uint64_t)gpioreg[PLLH_CTRL] & 0x3ff) + XOSC_FREQUENCY * (uint64_t)gpioreg[PLLH_FRAC] / (1 << 20))/(2*(gpioreg[PLLH_CTRL] >> 12)&0x7) ;
		break;
	}
	if(!pi_is_2711) // FixMe : Surely a register which say it is a 2x
		Freq*=2LL;
	Freq=Freq*(1.0-clk_ppm*1e-6);
	dbg_printf(1, "Pi4=%d Xosc = %llu Freq PLL no %d= %llu\n",pi_is_2711,XOSC_FREQUENCY,PllNo, Freq);

	return Freq;
}

int clkgpio::SetClkDivFrac(uint32_t Div, uint32_t Frac)
{

	gpioreg[GPCLK_DIV] = 0x5A000000 | ((Div) << 12) | Frac;
	usleep(100);	
	
	//dbg_printf(1, "Clk Number %d div %d frac %d\n", pllnumber, Div, Frac);
	
	return 0;
}

int clkgpio::SetMasterMultFrac(uint32_t Mult, uint32_t Frac)
{

	//dbg_printf(1,"Master Mult %d Frac %d\n",Mult,Frac);
	gpioreg[PLLC_CTRL] = (0x5a << 24) | (0x21 << 12) | Mult; //PDIV=1
	usleep(100);
	gpioreg[PLLC_FRAC] = 0x5A000000 | Frac;

	return 0;
}

int clkgpio::SetFrequency(double Frequency)
{
	if (ModulateFromMasterPLL)
	{

		double FloatMult=0;
		if(PllFixDivider==1) //Using PDIV thus frequency/2
		{
			if(pi_is_2711)
		 		FloatMult = ((double)(CentralFrequency + Frequency)*2 ) / ((double)(XOSC_FREQUENCY) * (1 - clk_ppm * 1e-6));
			 else
			 {
				FloatMult = ((double)(CentralFrequency + Frequency) ) / ((double)(XOSC_FREQUENCY*2) * (1 - clk_ppm * 1e-6));
			 }
			 
		}	 
		else
		{
			if(pi_is_2711)
		 		FloatMult = ((double)(CentralFrequency + Frequency)*PllFixDivider*2 ) / ((double)(XOSC_FREQUENCY) * (1 - clk_ppm * 1e-6)); 
			else
			{
				FloatMult = ((double)(CentralFrequency + Frequency)*PllFixDivider ) / ((double)(XOSC_FREQUENCY) * (1 - clk_ppm * 1e-6)); 
			}
				 
		}

		uint32_t freqctl = FloatMult * ((double)(1 << 20));
		int IntMultiply = freqctl >> 20; // Need to be calculated to have a center frequency
		freqctl &= 0xFFFFF;				 // Fractionnal is 20bits
		uint32_t FracMultiply = freqctl & 0xFFFFF;

		SetMasterMultFrac(IntMultiply, FracMultiply);
	}
	else
	{
		double Freqresult = (double)Pllfrequency / (double)(CentralFrequency + Frequency);
		uint32_t FreqDivider = (uint32_t)Freqresult;
		uint32_t FreqFractionnal = (uint32_t)(4096 * (Freqresult - (double)FreqDivider));
		if ((FreqDivider > 4096) || (FreqDivider < 2))
			dbg_printf(0, "Frequency out of range\n");
		dbg_printf(1,"DIV/FRAC %u/%u \n", FreqDivider, FreqFractionnal);
		
		SetClkDivFrac(FreqDivider, FreqFractionnal);
	}

	return 0;
}

uint32_t clkgpio::GetMasterFrac(double Frequency)
{
	if (ModulateFromMasterPLL)
	{
		double FloatMult=0;
		if((PllFixDivider==1))//There is no Prediv on Pi4 //Using PDIV thus frequency/2
		{
			if(pi_is_2711) // No PDIV on pi4
		 		FloatMult = ((double)(CentralFrequency + Frequency)*2 ) / ((double)(XOSC_FREQUENCY) * (1 - clk_ppm * 1e-6));
			else
			{
				FloatMult = ((double)(CentralFrequency + Frequency) ) / ((double)(XOSC_FREQUENCY*2) * (1 - clk_ppm * 1e-6));
			}
				 
		}	 
		else
		{
			if(pi_is_2711)
		 		FloatMult = ((double)(CentralFrequency + Frequency)*PllFixDivider*2 ) / ((double)(XOSC_FREQUENCY) * (1 - clk_ppm * 1e-6)); 
			else
			{
				FloatMult = ((double)(CentralFrequency + Frequency)*PllFixDivider ) / ((double)(XOSC_FREQUENCY) * (1 - clk_ppm * 1e-6)); 
			}
				 
		}	 
		uint32_t freqctl = FloatMult * ((double)(1 << 20));
		int IntMultiply = freqctl >> 20; // Need to be calculated to have a center frequency
		freqctl &= 0xFFFFF;				 // Fractionnal is 20bits
		uint32_t FracMultiply = freqctl & 0xFFFFF;
		return FracMultiply;
	}
	else
		return 0; //Not in Master CLk mode
}

int clkgpio::ComputeBestLO(uint64_t Frequency, int Bandwidth)
{
	// Algorithm adapted from https://github.com/SaucySoliton/PiFmRds/blob/master/src/pi_fm_rds.c
	// Choose an integer divider for GPCLK0
	//
	// There may be improvements possible to this algorithm.
	// Constants taken https://github.com/raspberrypi/linux/blob/ffd7bf4085b09447e5db96edd74e524f118ca3fe/drivers/clk/bcm/clk-bcm2835.c#L1763
	//MIN RATE is NORMALLY 600MHZ
	#define MIN_PLL_RATE 200e6
	#define MIN_PLL_RATE_USE_PDIV 1500e6 //1700 works but some ticky breaks in clock..PLL should be at limit
	#define MAX_PLL_RATE 4e9
	#define XTAL_RATE XOSC_FREQUENCY
	//Pi4 seems 54Mhz
	double xtal_freq_recip = 1.0 / XTAL_RATE; // todo PPM correction
	int best_divider = 0;

	int solution_count = 0;
	//printf("carrier:%3.2f ",carrier_freq/1e6);
	int divider=0, min_int_multiplier, max_int_multiplier, fom, int_multiplier, best_fom = 0;
	double Multiplier=0.0;
	best_divider = 0;
	bool cross_boundary=false;

	
	if(Frequency<MIN_PLL_RATE/4095)
	{
		dbg_printf(1, "Frequency too low !!!!!!\n"); 
		return -1;
	} 
	if(Frequency*2>MAX_PLL_RATE)
	{
		dbg_printf(1, "Frequency too high !!!!!!\n");
		return -1;
	} 
	if(Frequency*2>MIN_PLL_RATE_USE_PDIV)
	{ 
		best_divider=1; // We will use PREDIV 2 for PLL
	}	
	else
	{
		for (divider = 4095; divider > 1; divider--)//1 is allowed only for MASH=0
		{
			if (Frequency * divider < MIN_PLL_RATE)
				continue; // widest accepted frequency range
			if (Frequency * divider > MIN_PLL_RATE_USE_PDIV) // By Experiment on Rpi3B
			{
				continue;
			}
			max_int_multiplier = ((int)((double)(Frequency + Bandwidth) * divider * xtal_freq_recip));
			min_int_multiplier = ((int)((double)(Frequency - Bandwidth) * divider * xtal_freq_recip));
			if (min_int_multiplier != max_int_multiplier)
			{
				//dbg_printf(1,"Warning : cross boundary frequency\n");
				best_divider=divider; 
				cross_boundary=true;
				continue; // don't cross integer boundary	
			}
			else
			{
				cross_boundary=false;
				best_divider=divider; 
				break;
			}

			
		}
	}
	if (best_divider!=0)
	{
		PllFixDivider = best_divider;
		
		if(cross_boundary) dbg_printf(1,"Warning : cross boundary frequency\n");
		dbg_printf(1, "Found PLL solution for frequency %4.1fMHz : divider:%d VCO: %4.1fMHz\n", (Frequency/1e6), PllFixDivider,(Frequency/1e6) *((PllFixDivider==1)?2.0:(double)PllFixDivider));
		return 0;
	}
	else
	{
		dbg_printf(1, "Central frequency not available !!!!!!\n");
		return -1;
	}
}

double clkgpio::GetFrequencyResolution()
{
	double res = 0;
	if (ModulateFromMasterPLL)
	{
		res = XOSC_FREQUENCY / (double)(1 << 20) / PllFixDivider;
	}
	else
	{
		double Freqresult = (double)Pllfrequency / (double)(CentralFrequency);
		uint32_t FreqDivider = (uint32_t)Freqresult;
		res = (Pllfrequency / (double)(FreqDivider + 1) - Pllfrequency / (double)(FreqDivider)) / 4096.0;
	}
	return res;
}

double clkgpio::GetRealFrequency(double Frequency)
{
	double FloatMult = ((double)(CentralFrequency + Frequency) * PllFixDivider) / (double)(XOSC_FREQUENCY);
	uint32_t freqctl = FloatMult * ((double)(1 << 20));
	int IntMultiply = freqctl >> 20; // Need to be calculated to have a center frequency
	freqctl &= 0xFFFFF;				 // Fractionnal is 20bits
	uint32_t FracMultiply = freqctl & 0xFFFFF;
	double RealFrequency = ((double)IntMultiply + (FracMultiply / (double)(1 << 20))) * (double)(XOSC_FREQUENCY) / PllFixDivider - (CentralFrequency + Frequency);
	return RealFrequency;
}

int clkgpio::SetCenterFrequency(uint64_t Frequency, int Bandwidth)
{
	CentralFrequency = Frequency;
	if (ModulateFromMasterPLL)
	{
		//Choose best PLLDiv and Div
		
		
		if(pi_is_2711)
		{
			ComputeBestLO(Frequency*2, Bandwidth); //FixeDivider update
			//PllFixDivider=PllFixDivider/2;
		}
		else
		{
			ComputeBestLO(Frequency, Bandwidth); //FixeDivider update
		}
		
		
		if(PllFixDivider==1)
		{
			//We will use PDIV by 2, means like we have a 2 times more	
			SetClkDivFrac(2, 0x0); // NO MASH !!!!
			dbg_printf(1, "Pll Fix Divider\n");
			
		}
		else
		{
			SetClkDivFrac(PllFixDivider, 0x0); // NO MASH !!!!
			
		}

		// Apply PREDIV for PLL or not
		uint32_t ana[4];
		for (int i = 3; i >= 0; i--)
		{
			ana[i] = gpioreg[(A2W_PLLC_ANA0 ) + i];
			usleep(100);
			//dbg_printf(1,"PLLC %d =%x\n",i,ana[i]);
			ana[i] &= ~(1<<14);
		}

		if(PllFixDivider==1) 
		{
			dbg_printf(1,"Use PLL Prediv\n");
			ana[1] |= (1 << 14); // use prediv means Frequency*2
		}
		else	
		{
		    ana[1]|=(0<<14); // No use prediv means Frequenc
		}
		/*
	 * ANA register setup is done as a series of writes to
	 * ANA3-ANA0, in that order.  This lets us write all 4
	 * registers as a single cycle of the serdes interface (taking
	 * 100 xosc clocks), whereas if we were to update ana0, 1, and
	 * 3 individually through their partial-write registers, each
	 * would be their own serdes cycle.
	 */
		for (int i = 3; i >= 0; i--)
		{
			ana[i]|=(0x5A << 24) ;
			gpioreg[(A2W_PLLC_ANA0 ) + i] = ana[i];
			//dbg_printf(1,"Write %d = %x\n",i,ana[i]);
			usleep(100);
			
		}
		/*
		for (int i = 3; i >= 0; i--)
		{
				dbg_printf(1,"PLLC after %d =%x\n",i,gpioreg[(A2W_PLLC_ANA0 ) + i]);
		}
		*/
		SetFrequency(0);
		usleep(100);
		if ((gpioreg[CM_LOCK] & CM_LOCK_FLOCKC) > 0)
			dbg_printf(1, "Master PLLC Locked\n");
		else
			dbg_printf(1, "Warning ! Master PLLC NOT Locked !!!!\n");
		
		usleep(100);
		gpioreg[GPCLK_CNTL] = 0x5A000000 | (Mash << 9) | pllnumber | (1 << 4); //4 is START CLK
		usleep(100);
		
		gpioreg[GPCLK_CNTL] = 0x5A000000 | (Mash << 9) | pllnumber | (1 << 4); //4 is START CLK
		usleep(100);
		
	}
	else
	{
		GetPllFrequency(pllnumber);											   // Be sure to get the master PLL frequency
		gpioreg[GPCLK_CNTL] = 0x5A000000 | (Mash << 9) | pllnumber | (1 << 4); //4 is START CLK
		
	}
	return 0;
}

void clkgpio::SetPhase(bool inversed)
{
	uint32_t StateBefore = clkgpio::gpioreg[GPCLK_CNTL];
	clkgpio::gpioreg[GPCLK_CNTL] = (0x5A << 24) | StateBefore | ((inversed ? 1 : 0) << 8) | 1 << 5;
	//clkgpio::gpioreg[GPCLK_CNTL_2] = (0x5A << 24) | StateBefore | ((inversed ? 1 : 0) << 8) | 1 << 5;
	clkgpio::gpioreg[GPCLK_CNTL] = (0x5A << 24) | StateBefore | ((inversed ? 1 : 0) << 8) | 0 << 5;
	//clkgpio::gpioreg[GPCLK_CNTL_2] = (0x5A << 24) | StateBefore | ((inversed ? 1 : 0) << 8) | 0 << 5;
}
//https://elinux.org/The_Undocumented_Pi
//Should inspect https://github.com/raspberrypi/linux/blob/ffd7bf4085b09447e5db96edd74e524f118ca3fe/drivers/clk/bcm/clk-bcm2835.c#L695
void clkgpio::SetAdvancedPllMode(bool Advanced)
{
	ModulateFromMasterPLL = Advanced;
	if (ModulateFromMasterPLL)
	{
		//We must change Clk dependant from PLLC as we will modulate it
		// switch the core over to PLLA
       gpioreg[CORECLK_DIV]  = (0x5a<<24) | (4<<12) ; // core div 4
       usleep(100);
       gpioreg[CORECLK_CNTL] = (0x5a<<24) | (1<<4) | (4); // run, src=PLLA

	   // switch the EMMC over to PLLD
      
       int clktmp;
       clktmp = gpioreg[EMMCCLK_CNTL];
       gpioreg[EMMCCLK_CNTL] = (0xF0F&clktmp) | (0x5a<<24) ; // clear run
       usleep(100);
       gpioreg[EMMCCLK_CNTL] = (0xF00&clktmp) | (0x5a<<24) | (6); // src=PLLD
       usleep(100);
       gpioreg[EMMCCLK_CNTL] = (0xF00&clktmp) | (0x5a<<24) | (1<<4) | (6); // run , src=PLLD

	   
		SetPllNumber(clk_pllc, 0);		 // Use PLL_C , Do not USE MASH which generates spurious
		
		gpioreg[CM_PLLC] = 0x5A00022A; // Enable PllC_PER
		usleep(100);

		
		gpioreg[PLLC_CORE0] = 0x5A000000|(1<<8);//Disable 
		if(pi_is_2711)
			gpioreg[PLLC_PER] = 0x5A000002; // Divisor by 2 (1 seems not working on pi4)
		else
		{
			gpioreg[PLLC_PER] = 0x5A000001; // Divisor 1 for max frequence
		}
			

		usleep(100);
	}
}

void clkgpio::SetPLLMasterLoop(int Ki,int Kp,int Ka)
{
		uint32_t ana[4];
		for (int i = 3; i >= 0; i--)
		{
			ana[i] = gpioreg[(A2W_PLLC_ANA0 ) + i];
			
		}
		//Fixe me : Should make a OR with old value
		ana[1]&=(uint32_t)~((0x7<<A2W_PLL_KI_SHIFT)|(0xF<<A2W_PLL_KP_SHIFT)|(0x7<<A2W_PLL_KA_SHIFT));
		ana[1]|=(Ki<<A2W_PLL_KI_SHIFT)|(Kp<<A2W_PLL_KP_SHIFT)|(Ka<<A2W_PLL_KA_SHIFT);
		dbg_printf(1,"Loop parameter =%x\n",ana[1]);
		for (int i = 3; i >= 0; i--)
		{
			gpioreg[(A2W_PLLC_ANA0 ) + i] = (0x5A << 24) | ana[i];
			usleep(100);
		}
		usleep(100)	;
	
	
}

void clkgpio::print_clock_tree(void)
{

	printf("PLLC_DIG0=%08x\n", gpioreg[(0x1020 / 4)]);
	printf("PLLC_DIG1=%08x\n", gpioreg[(0x1024 / 4)]);
	printf("PLLC_DIG2=%08x\n", gpioreg[(0x1028 / 4)]);
	printf("PLLC_DIG3=%08x\n", gpioreg[(0x102c / 4)]);
	printf("PLLC_ANA0=%08x\n", gpioreg[(0x1030 / 4)]);
	printf("PLLC_ANA1=%08x\n", gpioreg[(0x1034 / 4)]);
	printf("PLLC_ANA2=%08x\n", gpioreg[(0x1038 / 4)]);
	printf("PLLC_ANA3=%08x\n", gpioreg[(0x103c / 4)]);
	printf("PLLC_DIG0R=%08x\n", gpioreg[(0x1820 / 4)]);
	printf("PLLC_DIG1R=%08x\n", gpioreg[(0x1824 / 4)]);
	printf("PLLC_DIG2R=%08x\n", gpioreg[(0x1828 / 4)]);
	printf("PLLC_DIG3R=%08x\n", gpioreg[(0x182c / 4)]);

	printf("PLLA_ANA0=%08x\n", gpioreg[(0x1010 / 4)]);
	printf("PLLA_ANA1=%08x prediv=%d\n", gpioreg[(0x1014 / 4)], (gpioreg[(0x1014 / 4)] >> 14) & 1);
	printf("PLLA_ANA2=%08x\n", gpioreg[(0x1018 / 4)]);
	printf("PLLA_ANA3=%08x\n", gpioreg[(0x101c / 4)]);

	printf("GNRIC CTL=%08x DIV=%8x  ", gpioreg[0], gpioreg[1]);
	printf("VPU   CTL=%08x DIV=%8x\n", gpioreg[2], gpioreg[3]);
	printf("SYS   CTL=%08x DIV=%8x  ", gpioreg[4], gpioreg[5]);
	printf("PERIA CTL=%08x DIV=%8x\n", gpioreg[6], gpioreg[7]);
	printf("PERII CTL=%08x DIV=%8x  ", gpioreg[8], gpioreg[9]);
	printf("H264  CTL=%08x DIV=%8x\n", gpioreg[10], gpioreg[11]);
	printf("ISP   CTL=%08x DIV=%8x  ", gpioreg[12], gpioreg[13]);
	printf("V3D   CTL=%08x DIV=%8x\n", gpioreg[14], gpioreg[15]);

	printf("CAM0  CTL=%08x DIV=%8x  ", gpioreg[16], gpioreg[17]);
	printf("CAM1  CTL=%08x DIV=%8x\n", gpioreg[18], gpioreg[19]);
	printf("CCP2  CTL=%08x DIV=%8x  ", gpioreg[20], gpioreg[21]);
	printf("DSI0E CTL=%08x DIV=%8x\n", gpioreg[22], gpioreg[23]);
	printf("DSI0P CTL=%08x DIV=%8x  ", gpioreg[24], gpioreg[25]);
	printf("DPI   CTL=%08x DIV=%8x\n", gpioreg[26], gpioreg[27]);
	printf("GP0   CTL=%08x DIV=%8x  ", gpioreg[0x70 / 4], gpioreg[0x74 / 4]);
	printf("GP1   CTL=%08x DIV=%8x\n", gpioreg[30], gpioreg[31]);

	printf("GP2   CTL=%08x DIV=%8x  ", gpioreg[32], gpioreg[33]);
	printf("HSM   CTL=%08x DIV=%8x\n", gpioreg[34], gpioreg[35]);
	printf("OTP   CTL=%08x DIV=%8x  ", gpioreg[36], gpioreg[37]);
	printf("PCM   CTL=%08x DIV=%8x\n", gpioreg[38], gpioreg[39]);
	printf("PWM   CTL=%08x DIV=%8x  ", gpioreg[40], gpioreg[41]);
	printf("SLIM  CTL=%08x DIV=%8x\n", gpioreg[42], gpioreg[43]);
	printf("SMI   CTL=%08x DIV=%8x  ", gpioreg[44], gpioreg[45]);
	printf("SMPS  CTL=%08x DIV=%8x\n", gpioreg[46], gpioreg[47]);

	printf("TCNT  CTL=%08x DIV=%8x  ", gpioreg[48], gpioreg[49]);
	printf("TEC   CTL=%08x DIV=%8x\n", gpioreg[50], gpioreg[51]);
	printf("TD0   CTL=%08x DIV=%8x  ", gpioreg[52], gpioreg[53]);
	printf("TD1   CTL=%08x DIV=%8x\n", gpioreg[54], gpioreg[55]);

	printf("TSENS CTL=%08x DIV=%8x  ", gpioreg[56], gpioreg[57]);
	printf("TIMER CTL=%08x DIV=%8x\n", gpioreg[58], gpioreg[59]);
	printf("UART  CTL=%08x DIV=%8x  ", gpioreg[60], gpioreg[61]);
	printf("VEC   CTL=%08x DIV=%8x\n", gpioreg[62], gpioreg[63]);

	printf("PULSE CTL=%08x DIV=%8x  ", gpioreg[100], gpioreg[101]);
	printf("PLLT  CTL=%08x DIV=????????\n", gpioreg[76]);

	printf("DSI1E CTL=%08x DIV=%8x  ", gpioreg[86], gpioreg[87]);
	printf("DSI1P CTL=%08x DIV=%8x\n", gpioreg[88], gpioreg[89]);
	printf("AVE0  CTL=%08x DIV=%8x\n", gpioreg[90], gpioreg[91]);

	printf("CMPLLA=%08x  ", gpioreg[0x104 / 4]);
	printf("CMPLLC=%08x \n", gpioreg[0x108 / 4]);
	printf("CMPLLD=%08x   ", gpioreg[0x10C / 4]);
	printf("CMPLLH=%08x \n", gpioreg[0x110 / 4]);

	printf("EMMC  CTL=%08x DIV=%8x\n", gpioreg[112], gpioreg[113]);
	printf("EMMC  CTL=%08x DIV=%8x\n", gpioreg[112], gpioreg[113]);
	printf("EMMC  CTL=%08x DIV=%8x\n", gpioreg[112], gpioreg[113]);

	// Sometimes calculated frequencies are off by a factor of 2
	// ANA1 bit 14 may indicate that a /2 prescaler is active
	double xoscmhz=XOSC_FREQUENCY/1e6;
	printf("PLLA PDIV=%d NDIV=%d FRAC=%d  ", (gpioreg[PLLA_CTRL] >> 12)&0x7, gpioreg[PLLA_CTRL] & 0x3ff, gpioreg[PLLA_FRAC]);
	printf(" %f MHz\n", xoscmhz * ((float)(gpioreg[PLLA_CTRL] & 0x3ff) + ((float)gpioreg[PLLA_FRAC]) / ((float)(1 << 20))));
	printf("DSI0=%d CORE=%d PER=%d CCP2=%d\n\n", gpioreg[PLLA_DSI0], gpioreg[PLLA_CORE], gpioreg[PLLA_PER], gpioreg[PLLA_CCP2]);

	printf("PLLB PDIV=%d NDIV=%d FRAC=%d  ", (gpioreg[PLLB_CTRL] >> 12)&0x7, gpioreg[PLLB_CTRL] & 0x3ff, gpioreg[PLLB_FRAC]);
	printf(" %f MHz\n", xoscmhz * ((float)(gpioreg[PLLB_CTRL] & 0x3ff) + ((float)gpioreg[PLLB_FRAC]) / ((float)(1 << 20))));
	printf("ARM=%d SP0=%d SP1=%d SP2=%d\n\n", gpioreg[PLLB_ARM], gpioreg[PLLB_SP0], gpioreg[PLLB_SP1], gpioreg[PLLB_SP2]);

	printf("PLLC PDIV=%d NDIV=%d FRAC=%d  ", (gpioreg[PLLC_CTRL] >> 12)&0x7, gpioreg[PLLC_CTRL] & 0x3ff, gpioreg[PLLC_FRAC]);
	printf(" %f MHz\n", xoscmhz * ((float)(gpioreg[PLLC_CTRL] & 0x3ff) + ((float)gpioreg[PLLC_FRAC]) / ((float)(1 << 20))));
	printf("CORE2=%d CORE1=%d PER=%d CORE0=%d\n\n", gpioreg[PLLC_CORE2], gpioreg[PLLC_CORE1], gpioreg[PLLC_PER], gpioreg[PLLC_CORE0]);

	printf("PLLD %x PDIV=%d NDIV=%d FRAC=%d  ", gpioreg[PLLD_CTRL], (gpioreg[PLLD_CTRL] >> 12)&0x7, gpioreg[PLLD_CTRL] & 0x3ff, gpioreg[PLLD_FRAC]);
	printf(" %f MHz\n", xoscmhz * ((float)(gpioreg[PLLD_CTRL] & 0x3ff) + ((float)gpioreg[PLLD_FRAC]) / ((float)(1 << 20))));
	printf("DSI0=%d CORE=%d PER=%d DSI1=%d\n\n", gpioreg[PLLD_DSI0], gpioreg[PLLD_CORE], gpioreg[PLLD_PER], gpioreg[PLLD_DSI1]);

	printf("PLLH PDIV=%d NDIV=%d FRAC=%d  ", (gpioreg[PLLH_CTRL] >> 12)&0x7, gpioreg[PLLH_CTRL] & 0x3ff, gpioreg[PLLH_FRAC]);
	printf(" %f MHz\n", xoscmhz * ((float)(gpioreg[PLLH_CTRL] & 0x3ff) + ((float)gpioreg[PLLH_FRAC]) / ((float)(1 << 20))));
	printf("AUX=%d RCAL=%d PIX=%d STS=%d\n\n", gpioreg[PLLH_AUX], gpioreg[PLLH_RCAL], gpioreg[PLLH_PIX], gpioreg[PLLH_STS]);
}

void clkgpio::enableclk(int gpio)
{
	switch (gpio)
	{
	case 4:
		gengpio.setmode(gpio, fsel_alt0);
		break;
	case 20:
		gengpio.setmode(gpio, fsel_alt5);
		break;
	case 32:
		gengpio.setmode(gpio, fsel_alt0);
		break;
	case 34:
		gengpio.setmode(gpio, fsel_alt0);
		break;
	//CLK2	
	case 6:
		gengpio.setmode(gpio, fsel_alt0);
		break;	
	default:
		dbg_printf(1, "gpio %d has no clk - available(4,20,32,34)\n", gpio);
		break;
	}
	usleep(100);
}

void clkgpio::disableclk(int gpio)
{
	gengpio.setmode(gpio, fsel_input);
}

void clkgpio::Setppm(double ppm)
{
	clk_ppm = ppm ; // -2 is empiric : FixMe
}

void clkgpio::SetppmFromNTP()
{
	struct timex ntx;
	int status;
	//Calibrate Clock system (surely depends also on PLL PPM
	// =====================================================

	ntx.modes = 0; /* only read */
	status = ntp_adjtime(&ntx);
	double ntp_ppm;

	if (status != TIME_OK)
	{
		dbg_printf(1, "Warning: NTP calibrate failed\n");
	}
	else
	{

		ntp_ppm = (double)ntx.freq / (double)(1 << 16);
		dbg_printf(1, "Info:NTP find offset %ld freq %ld pps=%ld ppm=%f\n", ntx.offset,ntx.freq,ntx.ppsfreq,ntp_ppm);
		
		if (fabs(ntp_ppm) < 200)
			Setppm(ntp_ppm/*+0.70*/); //0.7 is empiric 
			
	}
}

// ************************************** GENERAL GPIO *****************************************************

generalgpio::generalgpio() : gpio(/*GetPeripheralBase() + */GENERAL_BASE, GENERAL_LEN)
{
}

generalgpio::~generalgpio()
{
}

int generalgpio::setmode(uint32_t gpio, uint32_t mode)
{
	int reg, shift;

	reg = gpio / 10;
	shift = (gpio % 10) * 3;

	gpioreg[reg] = (gpioreg[reg] & ~(7 << shift)) | (mode << shift);

	return 0;
}

int generalgpio::setpulloff(uint32_t gpio)
{
	if(!pi_is_2711)
	{
		gpioreg[GPPUD]=0;
		usleep(150);	
		gpioreg[GPPUDCLK0]=1<<gpio;
		usleep(150);	
		gpioreg[GPPUDCLK0]=0;
	}
	else
	{
		uint32_t bits;
		uint32_t pull=0; // 0 OFF, 1 = UP, 2= DOWN
		int shift = (gpio & 0xf) << 1;
		bits = gpioreg[GPPUPPDN0 + (gpio>>4)];
		bits &= ~(3 << shift);
        bits |= (pull << shift);
      	gpioreg[GPPUPPDN0 + (gpio>>4)] = bits;
	}
	
	return 0;
}


// ********************************** PWM GPIO **********************************

pwmgpio::pwmgpio() : gpio(/*GetPeripheralBase() + */PWM_BASE, PWM_LEN)
{

	gpioreg[PWM_CTL] = 0;
}

pwmgpio::~pwmgpio()
{

	gpioreg[PWM_CTL] = 0;
	gpioreg[PWM_DMAC] = 0;
}

void pwmgpio::enablepwm(int gpio, int PwmNumber)
{
	if (PwmNumber == 0)
	{
		switch (gpio)
		{
		case 12:
			gengpio.setmode(gpio, fsel_alt0);
			break;
		case 18:
			gengpio.setmode(gpio, fsel_alt5);
			break;
		case 40:
			gengpio.setmode(gpio, fsel_alt0);
			break;

		default:
			dbg_printf(1, "gpio %d has no pwm - available(12,18,40)\n", gpio);
			break;
		}
	}
	if (PwmNumber == 1)
	{
		switch (gpio)
		{
		case 13:
			gengpio.setmode(gpio, fsel_alt0);
			break;
		case 19:
			gengpio.setmode(gpio, fsel_alt5);
			break;
		case 41:
			gengpio.setmode(gpio, fsel_alt0);
			break;
		case 45:
			gengpio.setmode(gpio, fsel_alt0);
			break;
		default:
			dbg_printf(1, "gpio %d has no pwm - available(13,19,41,45)\n", gpio);
			break;
		}
	}
	usleep(100);
}

void pwmgpio::disablepwm(int gpio)
{
	gengpio.setmode(gpio, fsel_input);
}

int pwmgpio::SetPllNumber(int PllNo, int MashType)
{
	if (PllNo < 8)
		pllnumber = PllNo;
	else
		pllnumber = clk_pllc;
	if (MashType < 4)
		Mash = MashType;
	else
		Mash = 0;
	clk.gpioreg[PWMCLK_CNTL] = 0x5A000000 | (Mash << 9) | pllnumber | (0 << 4); //4 is STOP CLK
	usleep(100);
	Pllfrequency = GetPllFrequency(pllnumber);
	return 0;
}

uint64_t pwmgpio::GetPllFrequency(int PllNo)
{
	
	return clk.GetPllFrequency(PllNo);
}

int pwmgpio::SetFrequency(uint64_t Frequency)
{
	Prediv = 32; // Fixe for now , need investigation if not 32 !!!! FixMe !
	double Freqresult = (double)Pllfrequency / (double)(Frequency * Prediv);
	uint32_t FreqDivider = (uint32_t)Freqresult;
	uint32_t FreqFractionnal = (uint32_t)(4096 * (Freqresult - (double)FreqDivider));
	if ((FreqDivider > 4096) || (FreqDivider < 2))
		dbg_printf(1, "Frequency out of range\n");
	dbg_printf(1, "PWM clk=%d / %d\n", FreqDivider, FreqFractionnal);
	clk.gpioreg[PWMCLK_DIV] = 0x5A000000 | ((FreqDivider) << 12) | FreqFractionnal;

	usleep(100);
	clk.gpioreg[PWMCLK_CNTL] = 0x5A000000 | (Mash << 9) | pllnumber | (1 << 4); //4 is STAR CLK
	usleep(100);

	SetPrediv(Prediv); //SetMode should be called before
	return 0;
}

void pwmgpio::SetMode(int Mode)
{
	if ((Mode >= pwm1pin) && (Mode <= pwm1pinrepeat))
		ModePwm = Mode;
}

int pwmgpio::SetPrediv(int predivisor) //Mode should be only for SYNC or a Data serializer : Todo
{
	Prediv = predivisor;
	if (Prediv > 32)
	{
		dbg_printf(1, "PWM Prediv is max 32\n");
		Prediv = 2;
	}
	dbg_printf(1, "PWM Prediv %d\n", Prediv);
	gpioreg[PWM_RNG1] = Prediv; // 250 -> 8KHZ
	usleep(100);
	gpioreg[PWM_RNG2] = Prediv; // 32 Mandatory for Serial Mode without gap

	//gpioreg[PWM_FIFO]=0xAAAAAAAA;

	gpioreg[PWM_DMAC] = PWMDMAC_ENAB | PWMDMAC_THRSHLD;
	usleep(100);
	gpioreg[PWM_CTL] = PWMCTL_CLRF;
	usleep(100);

	//gpioreg[PWM_CTL] = PWMCTL_USEF1| PWMCTL_MODE1| PWMCTL_PWEN1|PWMCTL_MSEN1;
	switch (ModePwm)
	{
	case pwm1pin:
		gpioreg[PWM_CTL] = PWMCTL_USEF1 | PWMCTL_MODE1 | PWMCTL_PWEN1 | PWMCTL_MSEN1;
		break; // All serial go to 1 pin
	case pwm2pin:
		gpioreg[PWM_CTL] = PWMCTL_USEF2 | PWMCTL_PWEN2 | PWMCTL_MODE2 | PWMCTL_USEF1 | PWMCTL_MODE1 | PWMCTL_PWEN1;
		break; // Alternate bit to pin 1 and 2
	case pwm1pinrepeat:
		gpioreg[PWM_CTL] = PWMCTL_USEF1 | PWMCTL_MODE1 | PWMCTL_PWEN1 | PWMCTL_RPTL1;
		break; // All serial go to 1 pin, repeat if empty : RF mode with PWM
	}
	usleep(100);

	return 0;
}

// ********************************** PCM GPIO (I2S) **********************************

pcmgpio::pcmgpio() : gpio(PCM_BASE, PCM_LEN)
{
	gpioreg[PCM_CS_A] = 1; // Disable Rx+Tx, Enable PCM block
}

pcmgpio::~pcmgpio()
{
}

int pcmgpio::SetPllNumber(int PllNo, int MashType)
{
	if (PllNo < 8)
		pllnumber = PllNo;
	else
		pllnumber = clk_pllc;
	if (MashType < 4)
		Mash = MashType;
	else
		Mash = 0;
	clk.gpioreg[PCMCLK_CNTL] = 0x5A000000 | (Mash << 9) | pllnumber | (1 << 4); //4 is START CLK
	Pllfrequency = GetPllFrequency(pllnumber);
	return 0;
}

uint64_t pcmgpio::GetPllFrequency(int PllNo)
{
	return clk.GetPllFrequency(PllNo);
}

int pcmgpio::ComputePrediv(uint64_t Frequency)
{
	int prediv = 5;
	for (prediv = 10; prediv < 1000; prediv++)
	{
		double Freqresult = (double)Pllfrequency / (double)(Frequency * prediv);
		if ((Freqresult < 4096.0) && (Freqresult > 2.0))
		{
			dbg_printf(1, "PCM prediv = %d\n", prediv);
			break;
		}
	}
	return prediv;
}

int pcmgpio::SetFrequency(uint64_t Frequency)
{
	Prediv = ComputePrediv(Frequency);
	double Freqresult = (double)Pllfrequency / (double)(Frequency * Prediv);
	uint32_t FreqDivider = (uint32_t)Freqresult;
	uint32_t FreqFractionnal = (uint32_t)(4096 * (Freqresult - (double)FreqDivider));
	dbg_printf(1, "PCM clk=%d / %d\n", FreqDivider, FreqFractionnal);
	if ((FreqDivider > 4096) || (FreqDivider < 2))
		dbg_printf(1, "PCM Frequency out of range\n");
	clk.gpioreg[PCMCLK_DIV] = 0x5A000000 | ((FreqDivider) << 12) | FreqFractionnal;
	SetPrediv(Prediv);
	return 0;
}

int pcmgpio::SetPrediv(int predivisor) //Carefull we use a 10 fixe divisor for now : frequency is thus f/10 as a samplerate
{
	if (predivisor > 1000)
	{
		dbg_printf(1, "PCM prediv should be <1000");
		predivisor = 1000;
	}

	gpioreg[PCM_TXC_A] = 0 << 31 | 1 << 30 | 0 << 20 | 0 << 16; // 1 channel, 8 bits
	usleep(100);

	//printf("Nb PCM STEP (<1000):%d\n",NbStepPCM);
	gpioreg[PCM_MODE_A] = (predivisor - 1) << 10; // SHOULD NOT EXCEED 1000 !!!
	usleep(100);
	gpioreg[PCM_CS_A] |= 1 << 4 | 1 << 3; // Clear FIFOs
	usleep(100);
	gpioreg[PCM_DREQ_A] = 64 << 24 | 64 << 8; //TX Fifo PCM=64 DMA Req when one slot is free? : Fixme 
	usleep(100);
	gpioreg[PCM_CS_A] |= 1 << 9; // Enable DMA
	usleep(100);
	gpioreg[PCM_CS_A] |= 1 << 2; //START TX PCM

	return 0;
}

// ********************************** PADGPIO (Amplitude) **********************************

padgpio::padgpio() : gpio(PADS_GPIO, PADS_GPIO_LEN)
{
}

padgpio::~padgpio()
{
}

int padgpio::setlevel(int level)
{
	gpioreg[PADS_GPIO_0]=(0x5a<<24) | (level&0x7) | (0<<4) | (0<<3);
	return 0;	
}
