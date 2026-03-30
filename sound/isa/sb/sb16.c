#include <sound/core/dev.h>
#include <hubble/printk.h>
#include <hpet/hpet.h>
#include <stdint.h>

//  SB16 I/O ports
#define SB16_BASE 0x220
#define SB16_RESET (SB16_BASE + 0x6)
#define SB16_READ (SB16_BASE + 0xA)
#define SB16_WRITE (SB16_BASE + 0xC)
#define SB16_READ_STAT (SB16_BASE + 0xE)
#define SB16_MIXER_ADDR (SB16_BASE + 0x4)
#define SB16_MIXER_DATA (SB16_BASE + 0x5)

// DSP commands
#define DSP_SET_RATE_OUT 0x41
#define DSP_OUT_8BIT 0xC0 // single-cycle, 8-bit PCM

// DMA channel 1 (8-bit)
#define DMA_MASK 0x0A
#define DMA_MODE 0x0B
#define DMA_FLIP 0x0C
#define DMA_ADDR1 0x02
#define DMA_COUNT1 0x03
#define DMA_PAGE1 0x83

// PCM buffer - must be below 16 MB and page-aligned
// The static buffer in .bss is guaranteed to go to the lower addresses
#define PCM_SAMPLE_RATE 22050
#define PCM_BUF_SAMPLES (PCM_SAMPLE_RATE / 2) // 0.5s maximum

static uint8_t pcm_buf[PCM_BUF_SAMPLES] __attribute__((aligned(4096)));

//  Port I/O
static inline void outb(uint16_t port, uint8_t val)
{
	__asm__ volatile("outb %0, %1" ::"a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t val;
	__asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
	return val;
}

// DSP helpers
static void dsp_write(uint8_t cmd)
{
	while (inb(SB16_WRITE) & 0x80)
		;
	outb(SB16_WRITE, cmd);
}

static int dsp_reset(void)
{
	outb(SB16_RESET, 1);
	for (volatile int i = 0; i < 1000; i++)
		;
	outb(SB16_RESET, 0);

	for (int i = 0; i < 1000; i++)
		if ((inb(SB16_READ_STAT) & 0x80) && inb(SB16_READ) == 0xAA)
			return 0;
	return -1;
}

//  Mixer: set the volume (0x00–0xFF)
static void mixer_set_volume(uint8_t vol)
{
	outb(SB16_MIXER_ADDR, 0x22); // master volume
	outb(SB16_MIXER_DATA, vol);
	outb(SB16_MIXER_ADDR, 0x04); // DAC volume
	outb(SB16_MIXER_DATA, vol);
}

// sin(x) ≈ through the quarter sine table (128 points from 0..255)
static const uint8_t sin_lut[128] = {
    128,
    134,
    140,
    146,
    152,
    158,
    164,
    170,
    176,
    181,
    187,
    192,
    197,
    202,
    207,
    211,
    215,
    219,
    223,
    226,
    229,
    232,
    234,
    236,
    238,
    240,
    241,
    242,
    243,
    244,
    244,
    244,
    244,
    244,
    243,
    242,
    241,
    240,
    238,
    236,
    234,
    232,
    229,
    226,
    223,
    219,
    215,
    211,
    207,
    202,
    197,
    192,
    187,
    181,
    176,
    170,
    164,
    158,
    152,
    146,
    140,
    134,
    128,
    122,
    116,
    110,
    104,
    98,
    92,
    86,
    80,
    75,
    69,
    64,
    59,
    54,
    49,
    45,
    41,
    37,
    33,
    30,
    27,
    24,
    22,
    20,
    18,
    16,
    15,
    14,
    13,
    12,
    12,
    12,
    12,
    12,
    13,
    14,
    15,
    16,
    18,
    20,
    22,
    24,
    27,
    30,
    33,
    37,
    41,
    45,
    49,
    54,
    59,
    64,
    69,
    75,
    80,
    86,
    92,
    98,
    104,
    110,
    116,
    122,
    128,
    128,
    128,
    128,
};

static uint8_t sin_sample(uint32_t phase)
{
	// phase: 0..511 = full sine wave cycle (512 samples per period)
	phase &= 0x1FF;
	if (phase < 128)
		return sin_lut[phase];
	else if (phase < 256)
		return sin_lut[255 - phase];
	else if (phase < 384)
		return 255 - sin_lut[phase - 256];
	else
		return 255 - sin_lut[511 - phase];
}

static void pcm_generate(uint32_t freq, uint32_t n_samples)
{
	// phase_step = freq * 512 / sample_rate
	uint32_t step = (freq * 512u) / PCM_SAMPLE_RATE;
	uint32_t phase = 0;

	for (uint32_t i = 0; i < n_samples; i++)
	{
		pcm_buf[i] = sin_sample(phase);
		phase = (phase + step) & 0x1FF;
	}
}

// DMA setup (channel 1, 8-bit)
static void dma_setup(uint32_t addr, uint32_t count)
{
	count--; /* DMA count = N-1 */

	outb(DMA_MASK, 0x05); // mask channel 1
	outb(DMA_FLIP, 0x00); // clear flip-flop
	outb(DMA_MODE, 0x49); // single, read, ch 1

	outb(DMA_ADDR1, (addr >> 0) & 0xFF);
	outb(DMA_ADDR1, (addr >> 8) & 0xFF);
	outb(DMA_PAGE1, (addr >> 16) & 0xFF);

	outb(DMA_COUNT1, (count >> 0) & 0xFF);
	outb(DMA_COUNT1, (count >> 8) & 0xFF);

	outb(DMA_MASK, 0x01); // unmask channel 1
}

//  sound_driver ops
static int sb16_init(void)
{
	if (dsp_reset() != 0)
	{
		printk("[sb16] reset failed\n");
		return -1;
	}
	mixer_set_volume(0xFF);
	printk("[sb16] init ok\n");
	return 0;
}

static void sb16_play(uint32_t freq, uint32_t duration_ms)
{
	if (freq == 0)
	{
		hpet_delay_ms(duration_ms);
		return;
	}

	// how many samples to play
	uint32_t n = (PCM_SAMPLE_RATE * duration_ms) / 1000;
	if (n > PCM_BUF_SAMPLES)
		n = PCM_BUF_SAMPLES;

	pcm_generate(freq, n);

	uint32_t addr = (uint32_t)(uintptr_t)pcm_buf;

	dma_setup(addr, n);

	// sample rate
	dsp_write(DSP_SET_RATE_OUT);
	dsp_write((PCM_SAMPLE_RATE >> 8) & 0xFF);
	dsp_write(PCM_SAMPLE_RATE & 0xFF);

	// start transmission: 8-bit unsigned mono
	dsp_write(DSP_OUT_8BIT);
	dsp_write(0x00); // mode: unsigned mono
	dsp_write((n - 1) & 0xFF);
	dsp_write((n - 1) >> 8);

	hpet_delay_ms(duration_ms);
}

static void sb16_stop(void)
{
	dsp_write(0xD0); // pause 8-bit DMA
}

const struct sound_driver sb16_driver = {
    .name = "sb16",
    .init = sb16_init,
    .play = sb16_play,
    .stop = sb16_stop,
};
