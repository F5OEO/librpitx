#ifndef DEF_GPIO
#define DEF_GPIO
#include "stdint.h"
#include <cstdio>

#define PHYSICAL_BUS 0x7E000000

class gpio
{
	public:
  bool pi_is_2711=false;	
  uint64_t XOSC_FREQUENCY=19200000; 
  public:
	volatile uint32_t *gpioreg = NULL;
	uint32_t gpiolen;
	gpio(uint32_t base, uint32_t len);
	~gpio();
	uint32_t GetPeripheralBase();
};

#define DMA_BASE (0x00007000)
#define DMA_LEN 0xF00

#define BCM2708_DMA_SRC_IGNOR (1 << 11)
#define BCM2708_DMA_SRC_INC (1 << 8)
#define BCM2708_DMA_DST_IGNOR (1 << 7)
#define BCM2708_DMA_NO_WIDE_BURSTS (1 << 26)
#define BCM2708_DMA_WAIT_RESP (1 << 3)
#define BCM2708_DMA_SET_INT (1 << 0)

#define BCM2708_DMA_D_DREQ (1 << 6)
#define BCM2708_DMA_PER_MAP(x) ((x) << 16)
#define BCM2708_DMA_END (1 << 1)
#define BCM2708_DMA_RESET (1 << 31)
#define BCM2708_DMA_ABORT (1 << 30)
#define BCM2708_DMA_INT (1 << 2)

#define DMA_CS (0x00 / 4)
#define DMA_CONBLK_AD (0x04 / 4)
#define DMA_DEBUG (0x20 / 4)

#define DMA_CS_RESET (1 << 31)
#define DMA_CS_ABORT (1 << 30)
#define DMA_CS_DISDEBUG (1 << 29)
#define DMA_CS_WAIT_FOR_OUTSTANDING_WRITES (1 << 28)
#define DMA_CS_INT (1 << 2)
#define DMA_CS_END (1 << 1)
#define DMA_CS_ACTIVE (1 << 0)
#define DMA_CS_PRIORITY(x) ((x)&0xf << 16)
#define DMA_CS_PANIC_PRIORITY(x) ((x)&0xf << 20)

class dmagpio : public gpio
{

  public:
	dmagpio();
};

//************************************ GENERAL GPIO ***************************************

#define GENERAL_BASE (0x00200000)
#define GENERAL_LEN 0xD0

#define GPFSEL0 (0x00 / 4)
#define GPFSEL1 (0x04 / 4)
#define GPFSEL2 (0x08 / 4)
#define GPPUD (0x94 / 4)
#define GPPUDCLK0 (0x98 / 4)
#define GPPUDCLK1 (0x9C / 4)

#define GPPUPPDN0 (0xBC/4)
#define GPPUPPDN1 (0xC0/4)
#define GPPUPPDN2 (0xC4/4)
#define GPPUPPDN3 (0xC8/4)

enum
{
	fsel_input,
	fsel_output,
	fsel_alt5,
	fsel_alt4,
	fsel_alt0,
	fsel_alt1,
	fsel_alt2,
	fsel_alt3
};

class generalgpio : public gpio
{

  public:
	generalgpio();
	int setmode(uint32_t gpio, uint32_t mode);
	~generalgpio();
	int setpulloff(uint32_t gpio);
};

// Add for PLL frequency CTRL wihout divider
// https://github.com/raspberrypi/linux/blob/rpi-4.9.y/drivers/clk/bcm/clk-bcm2835.c
// See interesting patch for jitter https://github.com/raspberrypi/linux/commit/76527b4e6a5dbe55e0b2d8ab533c2388b36c86be
#define GENMASK(h, l) (((U32_C(1) << ((h) - (l) + 1)) - 1) << (l))

#define CLK_BASE (0x00101000)
#define CLK_LEN 0x1660

#define CORECLK_CNTL (0x08 / 4)
#define CORECLK_DIV (0x0c / 4)
#define GPCLK_CNTL (0x70 / 4)
#define GPCLK_DIV (0x74 / 4)
#define GPCLK_CNTL_2 (0x80 / 4)
#define GPCLK_DIV_2 (0x84 / 4)
#define EMMCCLK_CNTL (0x1C0 / 4)
#define EMMCCLK_DIV (0x1C4 / 4)
#define CM_EMMC2CTL		(0x1d0/4)
#define CM_EMMC2DIV		(0x1d4/4)

#define CM_VPUCTL 0x008
#define CM_VPUDIV 0x00c
#define CM_SYSCTL 0x010
#define CM_SYSDIV 0x014
#define CM_PERIACTL 0x018
#define CM_PERIADIV 0x01c
#define CM_PERIICTL 0x020
#define CM_PERIIDIV 0x024
#define CM_H264CTL 0x028
#define CM_H264DIV 0x02c
#define CM_ISPCTL 0x030
#define CM_ISPDIV 0x034
#define CM_V3DCTL 0x038
#define CM_V3DDIV 0x03c
#define CM_CAM0CTL 0x040
#define CM_CAM0DIV 0x044
#define CM_CAM1CTL 0x048
#define CM_CAM1DIV 0x04c
#define CM_CCP2CTL 0x050
#define CM_CCP2DIV 0x054
#define CM_DSI0ECTL 0x058
#define CM_DSI0EDIV 0x05c
#define CM_DSI0PCTL 0x060
#define CM_DSI0PDIV 0x064
#define CM_DPICTL 0x068
#define CM_DPIDIV 0x06c
#define CM_GP0CTL 0x070
#define CM_GP0DIV 0x074
#define CM_GP1CTL 0x078
#define CM_GP1DIV 0x07c
#define CM_GP2CTL 0x080
#define CM_GP2DIV 0x084
#define CM_HSMCTL 0x088
#define CM_HSMDIV 0x08c
#define CM_OTPCTL 0x090
#define CM_OTPDIV 0x094
#define CM_PCMCTL 0x098
#define CM_PCMDIV 0x09c
#define CM_PWMCTL 0x0a0
#define CM_PWMDIV 0x0a4
#define CM_SLIMCTL 0x0a8
#define CM_SLIMDIV 0x0ac
#define CM_SMICTL 0x0b0
#define CM_SMIDIV 0x0b4
/* no definition for 0x0b8  and 0x0bc */
#define CM_TCNTCTL 0x0c0
#define CM_TCNT_SRC1_SHIFT 12
#define CM_TCNTCNT 0x0c4
#define CM_TECCTL 0x0c8
#define CM_TECDIV 0x0cc
#define CM_TD0CTL 0x0d0
#define CM_TD0DIV 0x0d4
#define CM_TD1CTL 0x0d8
#define CM_TD1DIV 0x0dc
#define CM_TSENSCTL 0x0e0
#define CM_TSENSDIV 0x0e4
#define CM_TIMERCTL 0x0e8
#define CM_TIMERDIV 0x0ec
#define CM_UARTCTL 0x0f0
#define CM_UARTDIV 0x0f4
#define CM_VECCTL 0x0f8
#define CM_VECDIV 0x0fc
#define CM_PULSECTL 0x190
#define CM_PULSEDIV 0x194
#define CM_SDCCTL 0x1a8
#define CM_SDCDIV 0x1ac
#define CM_ARMCTL 0x1b0
#define CM_AVEOCTL 0x1b8
#define CM_AVEODIV 0x1bc
#define CM_EMMCCTL 0x1c0
#define CM_EMMCDIV 0x1c4

#define CM_LOCK (0x114 / 4)
#define CM_LOCK_FLOCKH (1 << 12)
#define CM_LOCK_FLOCKD (1 << 11)
#define CM_LOCK_FLOCKC (1 << 10)
#define CM_LOCK_FLOCKB (1 << 9)
#define CM_LOCK_FLOCKA (1 << 8)

#define CM_PLLA (0x104 / 4)
/*
# define CM_PLL_ANARST			BIT(8)
# define CM_PLLA_HOLDPER		BIT(7)
# define CM_PLLA_LOADPER		BIT(6)
# define CM_PLLA_HOLDCORE		BIT(5)
# define CM_PLLA_LOADCORE		BIT(4)
# define CM_PLLA_HOLDCCP2		BIT(3)
# define CM_PLLA_LOADCCP2		BIT(2)
# define CM_PLLA_HOLDDSI0		BIT(1)
# define CM_PLLA_LOADDSI0		BIT(0)
*/
#define CM_PLLC (0x108 / 4)
#define CM_PLLD (0x10c / 4)
#define CM_PLLH (0x110 / 4)
#define CM_PLLB (0x170 / 4)

#define A2W_PLLA_ANA0 (0x1010 / 4)
#define A2W_PLLC_ANA0 (0x1030 / 4)
#define A2W_PLLD_ANA0 (0x1050 / 4)
#define A2W_PLLH_ANA0 (0x1070 / 4)
#define A2W_PLLB_ANA0 (0x10f0 / 4)
#define A2W_PLL_KA_SHIFT 7
#define A2W_PLL_KI_SHIFT 19
#define A2W_PLL_KP_SHIFT 15

#define PLLA_CTRL (0x1100 / 4)
#define PLLA_FRAC (0x1200 / 4)
#define PLLA_DSI0 (0x1300 / 4)
#define PLLA_CORE (0x1400 / 4)
#define PLLA_PER (0x1500 / 4)
#define PLLA_CCP2 (0x1600 / 4)

#define PLLB_CTRL (0x11e0 / 4)
#define PLLB_FRAC (0x12e0 / 4)
#define PLLB_ARM (0x13e0 / 4)
#define PLLB_SP0 (0x14e0 / 4)
#define PLLB_SP1 (0x15e0 / 4)
#define PLLB_SP2 (0x16e0 / 4)

#define PLLC_CTRL (0x1120 / 4)
#define PLLC_FRAC (0x1220 / 4)
#define PLLC_CORE2 (0x1320 / 4)
#define PLLC_CORE1 (0x1420 / 4)
#define PLLC_PER (0x1520 / 4)
#define PLLC_CORE0 (0x1620 / 4)

#define PLLD_CTRL (0x1140 / 4)
#define PLLD_FRAC (0x1240 / 4)
#define PLLD_DSI0 (0x1340 / 4)
#define PLLD_CORE (0x1440 / 4)
#define PLLD_PER (0x1540 / 4)
#define PLLD_DSI1 (0x1640 / 4)

#define PLLH_CTRL (0x1160 / 4)
#define PLLH_FRAC (0x1260 / 4)
#define PLLH_AUX (0x1360 / 4)
#define PLLH_RCAL (0x1460 / 4)
#define PLLH_PIX (0x1560 / 4)
#define PLLH_STS (0x1660 / 4)

#define XOSC_CTRL (0x1190 / 4)
//#define XOSC_FREQUENCY 19200000
//#define XOSC_FREQUENCY 54000000
//Parent PLL
enum
{
	clk_gnd,
	clk_osc,
	clk_debug0,
	clk_debug1,
	clk_plla,
	clk_pllc,
	clk_plld,
	clk_hdmi
};

class clkgpio : public gpio
{
  protected:
	int pllnumber;
	int Mash;
	uint64_t Pllfrequency;
	bool ModulateFromMasterPLL = false;
	uint64_t CentralFrequency = 0;
	generalgpio gengpio;
	double clk_ppm = 0;

  public:
	int PllFixDivider = 8; //Fix divider from the master clock in advanced mode

	clkgpio();
	~clkgpio();
	int SetPllNumber(int PllNo, int MashType);
	uint64_t GetPllFrequency(int PllNo);
	void print_clock_tree(void);
	int SetFrequency(double Frequency);
	int SetClkDivFrac(uint32_t Div, uint32_t Frac);
	void SetPhase(bool inversed);
	void SetAdvancedPllMode(bool Advanced);
	int SetCenterFrequency(uint64_t Frequency, int Bandwidth);
	double GetFrequencyResolution();
	double GetRealFrequency(double Frequency);
	int ComputeBestLO(uint64_t Frequency, int Bandwidth);
	int SetMasterMultFrac(uint32_t Mult, uint32_t Frac);
	uint32_t GetMasterFrac(double Frequency);
	void enableclk(int gpio);
	void disableclk(int gpio);
	void Setppm(double ppm);
	void SetppmFromNTP();
	void SetPLLMasterLoop(int Ki, int Kp, int Ka);
};

//************************************ PWM GPIO ***************************************

#define PWM_BASE (0x0020C000)
#define PWM_LEN 0x28

#define PWM_CTL (0x00 / 4)
#define PWM_DMAC (0x08 / 4)
#define PWM_RNG1 (0x10 / 4)
#define PWM_RNG2 (0x20 / 4)
#define PWM_FIFO (0x18 / 4)

#define PWMCLK_CNTL (40) // Clk register
#define PWMCLK_DIV (41)  // Clk register

#define PWMCTL_MSEN2 (1 << 15)
#define PWMCTL_USEF2 (1 << 13)
#define PWMCTL_RPTL2 (1 << 10)
#define PWMCTL_MODE2 (1 << 9)
#define PWMCTL_PWEN2 (1 << 8)

#define PWMCTL_MSEN1 (1 << 7)
#define PWMCTL_CLRF (1 << 6)
#define PWMCTL_USEF1 (1 << 5)
#define PWMCTL_POLA1 (1 << 4)
#define PWMCTL_RPTL1 (1 << 2)
#define PWMCTL_MODE1 (1 << 1)
#define PWMCTL_PWEN1 (1 << 0)
#define PWMDMAC_ENAB (1 << 31)
#define PWMDMAC_THRSHLD ((15 << 8) | (15 << 0))
enum pwmmode
{
	pwm1pin,
	pwm2pin,
	pwm1pinrepeat
};

class pwmgpio : public gpio
{
  protected:
	clkgpio clk;
	int pllnumber;
	int Mash;
	int Prediv; //Range of PWM
	uint64_t Pllfrequency;
	bool ModulateFromMasterPLL = false;
	int ModePwm = pwm1pin;
	generalgpio gengpio;

  public:
	pwmgpio();
	~pwmgpio();
	int SetPllNumber(int PllNo, int MashType);
	uint64_t GetPllFrequency(int PllNo);
	int SetFrequency(uint64_t Frequency);
	int SetPrediv(int predivisor);
	void SetMode(int Mode);
	void enablepwm(int gpio, int PwmNumber);
	void disablepwm(int gpio);
};

//******************************* PCM GPIO (I2S) ***********************************
#define PCM_BASE (0x00203000)
#define PCM_LEN 0x24

#define PCM_CS_A (0x00 / 4)
#define PCM_FIFO_A (0x04 / 4)
#define PCM_MODE_A (0x08 / 4)
#define PCM_RXC_A (0x0c / 4)
#define PCM_TXC_A (0x10 / 4)
#define PCM_DREQ_A (0x14 / 4)
#define PCM_INTEN_A (0x18 / 4)
#define PCM_INT_STC_A (0x1c / 4)
#define PCM_GRAY (0x20 / 4)

#define PCMCLK_CNTL (38) // Clk register
#define PCMCLK_DIV (39)  // Clk register

class pcmgpio : public gpio
{
  protected:
	clkgpio clk;
	int pllnumber;
	int Mash;
	int Prediv; //Range of PCM

	uint64_t Pllfrequency;
	int SetPrediv(int predivisor);

  public:
	pcmgpio();
	~pcmgpio();
	int SetPllNumber(int PllNo, int MashType);
	uint64_t GetPllFrequency(int PllNo);
	int SetFrequency(uint64_t Frequency);
	int ComputePrediv(uint64_t Frequency);
};

//******************************* PAD GPIO (Amplitude) ***********************************
#define PADS_GPIO (0x00100000)
#define PADS_GPIO_LEN (0x40 / 4)

#define PADS_GPIO_0 (0x2C / 4)
#define PADS_GPIO_1 (0x30 / 4)
#define PADS_GPIO_2 (0x34 / 4)

class padgpio : public gpio
{

  public:
	padgpio();
	~padgpio();
	int setlevel(int level);
};

enum dma_common_reg
{
	dma_pllc_frac = 0x7E000000 + (PLLC_FRAC << 2) + CLK_BASE,
	dma_pwm = 0x7E000000 + (PWM_FIFO << 2) + PWM_BASE,
	dma_pcm = 0x7E000000 + (PCM_FIFO_A << 2) + PCM_BASE,
	dma_fsel = 0x7E000000 + (GPFSEL0 << 2) + GENERAL_BASE,
	dma_pad = 0x7E000000 + (PADS_GPIO_0 << 2) + PADS_GPIO
};

#endif
