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
#include "atv.h"
#include "gpio.h"
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sched.h>
#include <stdlib.h>

atv::atv(uint64_t TuneFrequency, uint32_t SR, int Channel, uint32_t Lines) : bufferdma(Channel, 20 + /*(fixme)*/ 2 + Lines * (6 + (52 * 2)), 2, 1)
// Need 2 more bytes for 0 and 1
// Need 6 CB more for sync, if so as 2CBby sample : 3
{

    SampleRate = SR;
    tunefreq = TuneFrequency;
    clkgpio::SetAdvancedPllMode(true);
    clkgpio::SetCenterFrequency(TuneFrequency, SampleRate);
    clkgpio::SetFrequency(0);
    clkgpio::enableclk(4); // GPIO 4 CLK by default
    syncwithpwm = false;

    if (syncwithpwm)
    {
        pwmgpio::SetPllNumber(clk_plld, 1);
        pwmgpio::SetFrequency(SampleRate);
    }
    else
    {
        pcmgpio::SetPllNumber(clk_plld, 1);
        pcmgpio::SetFrequency(SampleRate);
    }

    padgpio pad;
    Originfsel = pad.gpioreg[PADS_GPIO_0];

    sampletab[(buffersize * registerbysample - 2)] = (0x5A << 24) + (1 & 0x7) + (1 << 4) + (0 << 3); // Amp 1
    sampletab[(buffersize * registerbysample - 1)] = (0x5A << 24) + (0 & 0x7) + (1 << 4) + (0 << 3); // Amp 0
    sampletab[(buffersize * registerbysample - 3)] = (0x5A << 24) + (4 & 0x7) + (1 << 4) + (0 << 3); // Amp 1

    SetDmaAlgo();
}

atv::~atv()
{
    clkgpio::disableclk(4);
    padgpio pad;
    pad.gpioreg[PADS_GPIO_0] = Originfsel;
}

void atv::SetDmaAlgo()
{
    dma_cb_t *cbp = cbarray;
    int LineResolution = 625;

    uint32_t level0= mem_virt_to_phys(&usermem[(buffersize * registerbysample - 1)]);
    uint32_t level1= mem_virt_to_phys(&usermem[(buffersize * registerbysample - 2)]);
    uint32_t level4= mem_virt_to_phys(&usermem[(buffersize * registerbysample - 3)]);
    
    int shortsync_0=2;
    int shortsync_1=30;

    int longsync_0=30;
    int longsync_1=2;

    int normalsync_0=4;
    int normalsync_1=6;

    int frontsync_1=2;

    for (int frame = 0; frame < 2; frame++)
    {
       //Preegalisation
        for (int i = 0; i < 6/*-frame*/; i++)
        {
            //2us 0,30us 1
            //@0
            //SYNC preegalisation 2us
            cbp->info = 0;                                                              //BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
            cbp->src = level0; //Amp 0
            cbp->dst = 0x7E000000 + (PADS_GPIO_0 << 2) + PADS_GPIO;
            cbp->length = 4;
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;

            // Delay
            if (syncwithpwm)
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PWM);
            else
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PCM_TX);
            cbp->src = mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed the PWM
            if (syncwithpwm)
                cbp->dst = 0x7E000000 + (PWM_FIFO << 2) + PWM_BASE;
            else
                cbp->dst = 0x7E000000 + (PCM_FIFO_A << 2) + PCM_BASE;
            cbp->length = 4 * shortsync_0; //2us
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;

            //SYNC preegalisation 30us
            cbp->info = 0;                                                              //BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
            cbp->src = level1; //Amp 1
            cbp->dst = 0x7E000000 + (PADS_GPIO_0 << 2) + PADS_GPIO;
            cbp->length = 4;
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;

            // Delay
            if (syncwithpwm)
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PWM);
            else
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PCM_TX);
            cbp->src = mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed the PWM
            if (syncwithpwm)
                cbp->dst = 0x7E000000 + (PWM_FIFO << 2) + PWM_BASE;
            else
                cbp->dst = 0x7E000000 + (PCM_FIFO_A << 2) + PCM_BASE;
            cbp->length = 4 * shortsync_1; //30us
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;
        }
        //SYNC top trame
        for (int i = 0; i < 5; i++)
        {
            cbp->info = 0;                                                              //BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
            cbp->src = level0; //Amp 0 27us
            cbp->dst = 0x7E000000 + (PADS_GPIO_0 << 2) + PADS_GPIO;
            cbp->length = 4;
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;

            // Delay
            if (syncwithpwm)
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PWM);
            else
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PCM_TX);
            cbp->src = mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed the PWM
            if (syncwithpwm)
                cbp->dst = 0x7E000000 + (PWM_FIFO << 2) + PWM_BASE;
            else
                cbp->dst = 0x7E000000 + (PCM_FIFO_A << 2) + PCM_BASE;
            cbp->length = 4 * longsync_0; //27us
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;

            cbp->info = 0;                                                              //BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
            cbp->src = level1; //Amp 1 5us
            cbp->dst = 0x7E000000 + (PADS_GPIO_0 << 2) + PADS_GPIO;
            cbp->length = 4;
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;

            // Delay
            if (syncwithpwm)
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PWM);
            else
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PCM_TX);
            cbp->src = mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed the PWM
            if (syncwithpwm)
                cbp->dst = 0x7E000000 + (PWM_FIFO << 2) + PWM_BASE;
            else
                cbp->dst = 0x7E000000 + (PCM_FIFO_A << 2) + PCM_BASE;
            cbp->length = 4 * longsync_1; //5us
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;
        }
        //postegalisation ; copy paste from preegalisation
        for (int i = 0; i < 5/*-i*/; i++)
        {
            //2us 0,30us 1
            //@0
            //SYNC preegalisation 2us
            cbp->info = 0;                                                              //BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
            cbp->src = level0; //Amp 0
            cbp->dst = 0x7E000000 + (PADS_GPIO_0 << 2) + PADS_GPIO;
            cbp->length = 4;
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;

            // Delay
            if (syncwithpwm)
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PWM);
            else
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PCM_TX);
            cbp->src = mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed the PWM
            if (syncwithpwm)
                cbp->dst = 0x7E000000 + (PWM_FIFO << 2) + PWM_BASE;
            else
                cbp->dst = 0x7E000000 + (PCM_FIFO_A << 2) + PCM_BASE;
            cbp->length = 4 * shortsync_0; //2us
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;

            //SYNC preegalisation 30us
            cbp->info = 0;                                                              //BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
            cbp->src = level1; //Amp 1
            cbp->dst = 0x7E000000 + (PADS_GPIO_0 << 2) + PADS_GPIO;
            cbp->length = 4;
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;

            // Delay
            if (syncwithpwm)
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PWM);
            else
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PCM_TX);
            cbp->src = mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed the PWM
            if (syncwithpwm)
                cbp->dst = 0x7E000000 + (PWM_FIFO << 2) + PWM_BASE;
            else
                cbp->dst = 0x7E000000 + (PCM_FIFO_A << 2) + PCM_BASE;
            cbp->length = 4 * shortsync_1; //30us
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;
        }

        for (int line = 0; line < /*305*/304+frame; line++)
        {
            
            //@0
            //SYNC 0/ 5us
            cbp->info = 0;                                                              //BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
            cbp->src = level0; //Amp 0
            cbp->dst = 0x7E000000 + (PADS_GPIO_0 << 2) + PADS_GPIO;
            cbp->length = 4;
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;

            // Delay
            if (syncwithpwm)
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PWM);
            else
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PCM_TX);
            cbp->src = mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed the PWM
            if (syncwithpwm)
                cbp->dst = 0x7E000000 + (PWM_FIFO << 2) + PWM_BASE;
            else
                cbp->dst = 0x7E000000 + (PCM_FIFO_A << 2) + PCM_BASE;
            cbp->length = 4 * normalsync_0; //4us
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;

            //@0
            //SYNC 1/ 5us
            cbp->info = 0;                                                              //BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
            cbp->src = level1; //Amp 1
            cbp->dst = 0x7E000000 + (PADS_GPIO_0 << 2) + PADS_GPIO;
            cbp->length = 4;
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;

            // Delay
            if (syncwithpwm)
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PWM);
            else
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PCM_TX);
            cbp->src = mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed the PWM
            if (syncwithpwm)
                cbp->dst = 0x7E000000 + (PWM_FIFO << 2) + PWM_BASE;
            else
                cbp->dst = 0x7E000000 + (PCM_FIFO_A << 2) + PCM_BASE;
            cbp->length = 4 * normalsync_1; //5us;
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;

            for (uint32_t samplecnt = 0; samplecnt < 52; samplecnt++) //52 us
            {
                sampletab[samplecnt * registerbysample] = (0x5A << 24) + (3 & 0x7) + (1 << 4) + (0 << 3); // Amplitude PAD
                                                                                                          //@0
                //DATA IN / 1us
                cbp->info = 0;                                                       //BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
                if(line<20) cbp->src=level1;
                //if((line>=20)&&(line<40)) cbp->src=level4;
                if(line>=20)    
                    cbp->src = mem_virt_to_phys(&usermem[samplecnt * registerbysample+frame*312*registerbysample]); //Amp 1

                cbp->dst = 0x7E000000 + (PADS_GPIO_0 << 2) + PADS_GPIO;
                cbp->length = 4;
                cbp->stride = 0;
                cbp->next = mem_virt_to_phys(cbp + 1);
                cbp++;

                // Delay
                if (syncwithpwm)
                    cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PWM);
                else
                    cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PCM_TX);
                cbp->src = mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed the PWM
                if (syncwithpwm)
                    cbp->dst = 0x7E000000 + (PWM_FIFO << 2) + PWM_BASE;
                else
                    cbp->dst = 0x7E000000 + (PCM_FIFO_A << 2) + PCM_BASE;
                cbp->length = 4; //1us
                cbp->stride = 0;
                cbp->next = mem_virt_to_phys(cbp + 1);
                cbp++;
            }
            
            //FRONT PORSH
            //SYNC 2us
            cbp->info = 0;                                                              //BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP  ;
            cbp->src = level1; //Amp 1
            cbp->dst = 0x7E000000 + (PADS_GPIO_0 << 2) + PADS_GPIO;
            cbp->length = 4;
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;

            // Delay
            if (syncwithpwm)
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PWM);
            else
                cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(DREQ_PCM_TX);
            cbp->src = mem_virt_to_phys(cbarray); // Data is not important as we use it only to feed the PWM
            if (syncwithpwm)
                cbp->dst = 0x7E000000 + (PWM_FIFO << 2) + PWM_BASE;
            else
                cbp->dst = 0x7E000000 + (PCM_FIFO_A << 2) + PCM_BASE;
            cbp->length = 4 * frontsync_1; //2us
            cbp->stride = 0;
            cbp->next = mem_virt_to_phys(cbp + 1);
            cbp++;
        }
    }
    cbp--;
    cbp->next = mem_virt_to_phys(cbarray); // We loop to the first CB
                                           //fprintf(stderr,"Last cbp :  src %x dest %x next %x\n",cbp->src,cbp->dst,cbp->next);
}

void atv::SetTvSample(uint32_t Index, float Amplitude) //-1;1
{
    Index = Index % buffersize;

    int IntAmplitude = round(abs(Amplitude) * 6.0) + 1; //1 to 7

    int IntAmplitudePAD = IntAmplitude;
    if (IntAmplitudePAD > 7)
        IntAmplitudePAD = 7;
    if (IntAmplitudePAD < 0)
        IntAmplitudePAD = 0;

    //fprintf(stderr,"Amplitude=%f PAD %d\n",Amplitude,IntAmplitudePAD);
    sampletab[(Index)*registerbysample] = (0x5A << 24) + (IntAmplitudePAD & 0x7) + (1 << 4) + (0 << 3); // Amplitude PAD

    PushSample(Index); // ??
}

void atv::SetTvSamples(float *sample, size_t Size)
{
    size_t NbWritten = 0;
    int OSGranularity = 100;

    long int start_time;
    long time_difference = 0;
    struct timespec gettime_now;

    while (NbWritten < Size)
    {
        clock_gettime(CLOCK_REALTIME, &gettime_now);
        start_time = gettime_now.tv_nsec;
        int Available = GetBufferAvailable();
        int TimeToSleep = 1e6 * ((int)buffersize * 3 / 4 - Available) / SampleRate - OSGranularity; // Sleep for theorically fill 3/4 of Fifo
        if (TimeToSleep > 0)
        {
            //fprintf(stderr,"buffer size %d Available %d SampleRate %d Sleep %d\n",buffersize,Available,SampleRate,TimeToSleep);
            usleep(TimeToSleep);
        }
        else
        {
            //fprintf(stderr,"No Sleep %d\n",TimeToSleep);
            sched_yield();
        }
        clock_gettime(CLOCK_REALTIME, &gettime_now);
        time_difference = gettime_now.tv_nsec - start_time;
        if (time_difference < 0)
            time_difference += 1E9;
        //fprintf(stderr,"Measure samplerate=%d\n",(int)((GetBufferAvailable()-Available)*1e9/time_difference));
        Available = GetBufferAvailable();
        int Index = GetUserMemIndex();
        int ToWrite = ((int)Size - (int)NbWritten) < Available ? Size - NbWritten : Available;

        for (int i = 0; i < ToWrite; i++)
        {
            SetTvSample(Index + i, sample[NbWritten++]);
        }
    }
}
