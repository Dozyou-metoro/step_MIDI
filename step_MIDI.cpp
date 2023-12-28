#include <cstdint>
#include <thread>
#include <cmath>
#include <vector>
#include <cinttypes>

#include <pigpio.h>

#include "errorprint.h"

#include "../L6470_pi/Stepper.hpp"

#include "../MIDIfile_io/tone_name.hpp"
#include "../MIDIfile_io/MIDI_input.hpp"

const uint32_t spi_speed = 1000000;
const uint8_t spi_ch = 0;
const uint8_t spi_ss1_pin = 25;
const uint8_t spi_ss2_pin = 8;
const uint8_t spi_busy1_pin=2;
const uint8_t spi_busy2_pin=3;

class step_MIDI_move
{
private:
    std::vector<uint8_t> motor_busy_status; // モーターの空き情報
    tone_name_array tone;                   // 音階名一覧
    uint32_t tempo_us = 0;                  // テンポ(四分音符あたりus)
    MIDI_input *midi;                       // MIDIクラスを入れておく



public:
    template <typename... TYPE>
    step_MIDI_move(MIDI_input *midi_class, TYPE... pins);

    ~step_MIDI_move();

    void step_play(MIDI_track_data::track_data_format MIDI_data);

    stepper_motor *motor;

};

template <typename... TYPE>
step_MIDI_move::step_MIDI_move(MIDI_input *midi_class, TYPE... pins)
{
    motor = new stepper_motor(spi_speed, spi_ch, pins...); // ステッピングモーター駆動クラスを定義

    for (int i = 0; i < motor->get_motor_num(); i++)
    {
        motor->send_data(i, 0x00, 0x00, 0x00, 0x00);

        motor->send_data(i, 0x05, 0x01, 0x00);
        motor->send_data(i, 0x06, 0x02, 0x20);
        motor->send_data(i, 0x07, 0x01, 0xC4);
        motor->send_data(i, 0x08, 0x00, 0x00);

        motor->send_data(i, 0x0A, 0xFF);
        motor->send_data(i, 0x0B, 0xFF);
        motor->send_data(i, 0x0C, 0xFF);

        motor->send_data(i, 0x13, 0x0F);
        motor->send_data(i, 0x0A, 0xff);
        motor->send_data(i, 0x16, 0x00);
    }

    motor_busy_status.resize(motor->get_motor_num() + 1); // モーター状態管理配列を生成

    midi = midi_class;

    for (int i = 0; i < midi->heard_data->MIDI_track_num; i++) // MIDIファイルを読んでテンポ情報を探す
    {                                                          // トラック数ぶん回す
        for (int j = 0;; j++)
        { // データの数だけ回す

            MIDI_track_data::track_data_format buf = midi->get_MIDI_data(0, j);
            if (buf.data_size == 0)
            {
                break;
            }

            if (buf.event_data[0] == 0xff && buf.event_data[1] == 0x51)
            { // テンポ情報なら上書き
                uint32_t tempo_buf = 0;
                for (int k = 0; k < 3; k++)
                {
                    tempo_buf += buf.event_data[3 + k] << (16 - 8 * k);
                }

                tempo_us = tempo_buf;
                printf("TEMPO:%" PRIx32 "\n", tempo_us);
            }
        }

       
    }

    
}

step_MIDI_move::~step_MIDI_move()
{

    delete motor;
}

void step_MIDI_move::step_play(MIDI_track_data::track_data_format MIDI_data)
{
    switch (MIDI_data.event_data[0] & 0xf0)
    {
    case 0x80: // 鳴らす指令
        for (int i = 0; i < motor_busy_status.size(); i++)
        {
            if (motor_busy_status[i] == MIDI_data.event_data[1]) // 指定の音を鳴らしているモーターがあれば止める
            {
                motor_busy_status[i] = 0;
                std::this_thread::sleep_for(std::chrono::microseconds((int)(tempo_us * ((double)MIDI_data.time / midi->heard_data->resolution))));
                motor->send_data(i, 0xB0);

                printf("MOTOR:%d ", i);
                for (int j = 0; j < MIDI_data.data_size; j++)
                {
                    printf("0x%02X,", MIDI_data.event_data[j]);
                }
                printf("\n");

                break;
            }
        }
        break;

    case 0x90: // 止める指令
        for (int i = 0; i < motor_busy_status.size(); i++)
        {
            motor_busy_status.back() = 0;  // free(溢れたときの予備)をリセット
            if (motor_busy_status[i] == 0) // 空いていれば音階情報を入れて回す
            {
                motor_busy_status[i] = MIDI_data.event_data[1];
                std::this_thread::sleep_for(std::chrono::microseconds((int)(tempo_us * ((double)MIDI_data.time / midi->heard_data->resolution))));
                
                motor->send_data(i, 0x51, ((int)(66 * tone.tone_frequency[MIDI_data.event_data[1]]) >> 16) & 0xff, ((int)(66 * tone.tone_frequency[MIDI_data.event_data[1]]) >> 8) & 0xff, (int)(66 * tone.tone_frequency[MIDI_data.event_data[1]]) & 0xff);

                printf("MOTOR:%d ", i);
                for (int j = 0; j < MIDI_data.data_size; j++)
                {
                    printf("0x%02X,", MIDI_data.event_data[j]);
                }
                printf("\n");

                break;
            }
        }
        break;

    default:
        printf("MOTOR:   ");
        for (int j = 0; j < MIDI_data.data_size; j++)
        {
            printf("0x%02X,", MIDI_data.event_data[j]);
        }
        printf("\n");
        break;
    }
}


int main(int argc, char **argv)
{
    if (argc < 2)
    {
        ERROR_EXIT("ファイルが足りません。", 0);
    }

    MIDI_input midi(argv[1]);

    step_MIDI_move step(&midi, spi_ss1_pin, spi_busy1_pin,spi_ss2_pin,spi_busy2_pin);



    for (int i = 0;; i++)
    {
        MIDI_track_data::track_data_format buf = midi.get_MIDI_data(0, i);
        if (buf.data_size == 0)
        {
            break;
        }

        step.step_play(buf);
    }
}
