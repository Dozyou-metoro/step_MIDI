#include <cstdint>
#include <thread>
#include <cmath>
#include <vector>

#include <pigpio.h>

#include "errorprint.h"

#include "../L6470_pi/Stepper.hpp"

#include "../MIDIfile_io/tone_name.hpp"
#include "../MIDIfile_io/MIDI_input.hpp"

const uint32_t spi_speed = 1000000;
const uint8_t spi_ch = 0;
const double spi_ss1_pin = 25;

class step_MIDI_move
{
private:
    std::vector<uint8_t> motor_busy_status;//モーターの空き情報
    tone_name_array tone;//音階名一覧
    uint32_t tempo_ms = 500;//テンポ(四分音符あたりms)
    MIDI_input midi;//MIDIクラスを入れておく

public:
    template <typename... TYPE>
    step_MIDI_move(MIDI_input midi_class, TYPE... ss_pin);
    ~step_MIDI_move();

    void step_play(MIDI_track_data::track_data_format MIDI_data);

    stepper_motor *motor;
};

template <typename... TYPE>
step_MIDI_move::step_MIDI_move(MIDI_input midi_class, TYPE... ss_pin)
{
    motor = new stepper_motor(spi_speed, spi_ch, ss_pin...);//ステッピングモーター駆動クラスを定義
    motor_busy_status.resize(motor->get_motor_num() + 1);//モーター状態管理配列を生成

    midi = midi_class;

    for (int i = 0; i < midi.heard_data->MIDI_track_num; i++)//MIDIファイルを読んでテンポ情報を探す
    {//トラック数ぶん回す
        for (int j = 0; j < midi.track_data.size(); j++)
        {//データの数だけ回す

            MIDI_track_data::track_data_format buf = midi.track_data[i]->get_track_data(j);
            if (buf.event_data[0] == 0x51 && buf.event_data[1] == 0x03)
            {//テンポ情報なら上書き
                for (int k = 0; k < 3; k++)
                {
                    tempo_ms += buf.event_data[2 + k] << (16 - 8 * k);
                }
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
    case 0x80://止める指令
        for (int i = 0; i < motor_busy_status.size(); i++)
        {
            if (motor_busy_status[i] == MIDI_data.event_data[1])//指定の音を鳴らしているモーターがあれば止める
            {   
                motor_busy_status[i] = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(tempo_ms * ((double)MIDI_data.time / midi.heard_data->resolution)));
                motor->spi_write(i, 0x51, ((int)(66 * (double)tone[MIDI_data.event_data[1]]) >> 16) & 0xff, ((int)(66 * (double)tone[MIDI_data.event_data[1]]) >> 8) & 0xff, (int)(66 * (double)tone[MIDI_data.event_data[1]]) & 0xff);
                
                break;
            }
        }
        break;

    case 0x90://鳴らす指令
        for (int i = 0; i < motor_busy_status.size(); i++)
        {
            motor_busy_status.back() = 0;//free(溢れたときの予備)をリセット
            if (motor_busy_status[i] == 0)//空いていれば音階情報を入れて回す
            {
                motor_busy_status[i] = MIDI_data.event_data[1];
                std::this_thread::sleep_for(std::chrono::milliseconds(tempo_ms * ((double)MIDI_data.time / midi.heard_data->resolution)));

                motor->spi_write(i, 0x51, 0x00, 0x00, 0x00);
                break;
            }
        }
        break;

    default:
        break;
    }
}

int main(int argc, char **argv)
{
    MIDI_input midi(argv[1]);

    step_MIDI_move step(midi, spi_ss1_pin);

    for(int i=0;;i++){
        MIDI_track_data::track_data_format buf = midi.get_MIDI_data(0,i);
        if(buf.data_size == 0){
            break;
        }

        step.step_play(buf);
    }
}
