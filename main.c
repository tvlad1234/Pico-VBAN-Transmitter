#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/vreg.h"

#include "10baset.h"
#include "vban.h"

// Number of audio samples to buffer
#define AUDIO_BUFSIZE 64

// Selects the ADC channel to use (second channel is next one after it)
#define CAPTURE_CHANNEL 1

// Number of audio channels (max 2)
uint8_t audio_channels = 2;

// Size of the VBAN packet
uint audio_packetsize;

// Buttons
#define SR_BTN_PIN 11   // selects the sampling rate
#define CHAN_BTN_PIN 12 // toggles between stereo and mono

// IP address of the transmitter. Replace with one coresponding to your configuration.
uint8_t src_ip[4] = {192, 168, 1, 2};

// IP address of the receiver. Replace with one coresponding to your configuration.
dest_ip_t dst_ip = {192, 168, 1, 255};

// UDP port to send the data to
uint16_t dst_port = 6980;

// UDP payload and VBAN header
udp_payload payload;
T_VBAN_HEADER header;

// Pointer to audio data buffer
uint8_t *audio_pointer = payload.data + VBAN_HEADER_SIZE;

// DMA channel and config
dma_channel_config cfg;
uint dma_chan;

// Available sample rates
const uint sampleRates[] = {6, 8, 12, 16, 24, 32, 48, 64, 96, 128};
static uint8_t sampRateIndex = 6;

// Misc variables
uint8_t led_cnt;
bool led_status;

// Function prototypes
void sample(uint8_t *capture_buf);
void dma_handler(void);
void setup_adc_dma(uint num_channels);
void set_audio_sample_rate(T_VBAN_HEADER *header, uint samplerate, uint channels);
bool readSrButton(void);
bool readChanButton(void);

// DMA interrupt handler (runs after each audio buffer capture)
void dma_handler(void)
{
    // Stop the adc and anknowkedge the interrupt
    adc_run(false);
    dma_channel_acknowledge_irq0(dma_chan);

    // Increment VBAN packet counter and copy the header into the network payload
    vban_update_packet_count(&header);
    memcpy(payload.data, &header, VBAN_HEADER_SIZE);

    // Send the payload over the network
    udp_send_payload(&payload);

    // Blink LED
    if (led_cnt == 0)
    {
        gpio_put(PICO_DEFAULT_LED_PIN, led_status);
        led_status = !led_status;
    }
    led_cnt++;

    // Change sample rate on button press
    if (readSrButton())
    {
        if (sampRateIndex < 9)
            sampRateIndex++;
        else
            sampRateIndex = 0;
        set_audio_sample_rate(&header, sampleRates[sampRateIndex], audio_channels);
    }

    // Change number of channels on button press
    if (readChanButton())
    {
        if (audio_channels == 1)
            audio_channels = 2;
        else
            audio_channels = 1;
        set_audio_sample_rate(&header, sampleRates[sampRateIndex], audio_channels); // number of channels is set alongside the sampling rate
    }

    // Start again
    sample(audio_pointer);
}

int main()
{
    // turbo speed, rev up your engines :D
    vreg_set_voltage(VREG_VOLTAGE_1_15); // overvolt the core just a bit
    set_sys_clock_khz(270000, true);     // overclock to 270MHz (from 125MHz)

    // Initialize LED pin
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // Initialize button pins
    gpio_init(SR_BTN_PIN);
    gpio_set_dir(SR_BTN_PIN, GPIO_IN);
    gpio_pull_up(SR_BTN_PIN);

    gpio_init(CHAN_BTN_PIN);
    gpio_set_dir(CHAN_BTN_PIN, GPIO_IN);
    gpio_pull_up(CHAN_BTN_PIN);

    // Wait for button pins to go up
    sleep_ms(200);

    // Initialize the ADC, analog pins and DMA
    setup_adc_dma(audio_channels);

    // Initialize Ethernet
    eth_set_ip(src_ip[0], src_ip[1], src_ip[2], src_ip[3]);
    eth_core_start();

    // Construct the VBAN protocol header
    vban_construct_header(&header, 0, VBAN_PROTOCOL_AUDIO, AUDIO_BUFSIZE, audio_channels, VBAN_DATATYPE_BYTE8, "Pico");

    // Set the audio sampling rate
    set_audio_sample_rate(&header, sampleRates[sampRateIndex], audio_channels);

    // Set the parameters for the network payload
    payload.ip = dst_ip;
    payload.port = dst_port;
    payload.length = audio_packetsize;

    // Start the sampling loop
    sample(audio_pointer);

    while (1)
    {
        tight_loop_contents();
    }
}

bool readSrButton(void)
{
    static bool prev = false;
    bool state = gpio_get(SR_BTN_PIN);
    bool pressed = false;
    if (prev != state && !state)
        pressed = true;
    prev = state;
    return pressed;
}

bool readChanButton(void)
{
    static bool prev = false;
    bool state = gpio_get(CHAN_BTN_PIN);
    bool pressed = false;
    if (prev != state && !state)
        pressed = true;
    prev = state;
    return pressed;
}

// Set audio sample rate
void set_audio_sample_rate(T_VBAN_HEADER *header, uint samplerate, uint num_channels)
{
    // Set packet size according to number of channels
    audio_packetsize = (VBAN_HEADER_SIZE + AUDIO_BUFSIZE * audio_channels);

    // Set ADC sample rate
    adc_set_clkdiv(48000 / samplerate / num_channels);

    // Set sample rate label to use in VBAN packet header
    header->format_SR = vban_get_SR_from_list(samplerate * 1000);
    header->format_SR |= VBAN_PROTOCOL_AUDIO << 5; // sub-protocol
    header->format_nbc = audio_channels - 1;
}

// Initialize the ADC, analog pins and DMA
void setup_adc_dma(uint num_channels)
{
    // Initialize GPIO pin for every used channel
    adc_gpio_init(26 + CAPTURE_CHANNEL);
    adc_gpio_init(26 + 1 + CAPTURE_CHANNEL);

    // Initialize ADC
    adc_init();
    adc_fifo_setup(
        true,  // Write each completed conversion to the sample FIFO
        true,  // Enable DMA data request (DREQ)
        1,     // DREQ (and IRQ) asserted when at least 1 sample present
        false, // We won't see the ERR bit because of 8 bit reads; disable.
        true   // Shift each sample to 8 bits when pushing to FIFO
    );

    // Claim DMA channel
    dma_chan = dma_claim_unused_channel(true);
    cfg = dma_channel_get_default_config(dma_chan);

    // Reading from constant address, writing to incrementing byte addresses
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);

    // Pace transfers based on availability of ADC samples
    channel_config_set_dreq(&cfg, DREQ_ADC);

    // Enable IRQ for DMA channel
    dma_channel_set_irq0_enabled(dma_chan, true);

    // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

// Sample data from the ADC
void sample(uint8_t *capture_buf)
{
    // Select ADC inputs
    if (audio_channels >= 2)
        adc_set_round_robin(0x03 << CAPTURE_CHANNEL);
    else
        adc_set_round_robin(0x00);
    adc_select_input(CAPTURE_CHANNEL);

    // Drain ADC FIFO
    adc_fifo_drain();

    // Configure DMA channel for ADC
    dma_channel_configure(dma_chan, &cfg,
                          capture_buf,                    // dst
                          &adc_hw->fifo,                  // src
                          AUDIO_BUFSIZE * audio_channels, // transfer count
                          true                            // start immediately
    );

    // Start the conversion
    adc_run(true);
}