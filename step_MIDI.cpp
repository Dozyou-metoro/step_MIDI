#include <cstdint>
#include <thread>
#include <cmath>

#include <pigpio.h>

#include "errorprint.h"

const uint32_t spi_speed = 1000000;
const uint8_t spi_ch = 0;

const double spi_ss1_pin = 25;

const int f_sound = 440;

int spi_handle = 0;

void spi_write(int spi_ss1_pin);
template <typename... type> void spi_write(int SS_pin, char front_data, type... data);

int main(void)
{
    system("sudo killall pigpiod");

    if (gpioInitialise() < 0)
    {
        ERROR_EXIT("GPIOの初期化に失敗しました。", -1);
    }

    gpioSetMode(spi_ss1_pin, PI_OUTPUT);

    gpioWrite(spi_ss1_pin, PI_HIGH);

    spi_handle = spiOpen(spi_ch, spi_speed, 0);

    if (spi_handle < 0)
    {
        ERROR_EXIT("SPIの初期化に失敗しました。", -2);
    }

    spi_write(spi_ss1_pin, 0x00, 0x00, 0x00, 0x00);

    spi_write(spi_ss1_pin, 0x05,0x02,0x20);
    spi_write(spi_ss1_pin, 0x06,0x02,0x20);
    spi_write(spi_ss1_pin, 0x07,0x01,0xC4);
    spi_write(spi_ss1_pin, 0x08,0x00,0x00);
    
    spi_write(spi_ss1_pin, 0x0A,0xFF);
    spi_write(spi_ss1_pin, 0x0B,0xFF);
    spi_write(spi_ss1_pin, 0x0C,0xFF);

    spi_write(spi_ss1_pin, 0x13,0x0F);

    spi_write(spi_ss1_pin, 0x0A, 0xff);
    spi_write(spi_ss1_pin, 0x16, 0x00);

    while (1)
    {
        for (int i = 0; i < 1; i++)
        {
            spi_write(spi_ss1_pin, 0x51, ((int)(66 * (double)f_sound * std::pow(2, i)) >> 16) & 0xff, ((int)(66 * (double)f_sound * std::pow(2, i)) >> 8) & 0xff, (int)(66 * (double)f_sound * std::pow(2, i)) & 0xff);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    spiClose(spi_handle);
    gpioTerminate();
}

template <typename... type> void spi_write(int SS_pin, char front_data, type... data)
{
    gpioWrite(SS_pin, PI_LOW);
    spiWrite(spi_handle, &front_data, sizeof(char));
    gpioWrite(SS_pin, PI_HIGH);

    spi_write(SS_pin, data...);
}

void spi_write(int SS_pin)
{
    return;
}
