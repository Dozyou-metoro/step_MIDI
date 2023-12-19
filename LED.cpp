#include<pigpio.h>
#include<thread>
#include<chrono>

const unsigned int outpin=25;

int main(void){
    gpioInitialise();
    
    gpioSetMode(outpin,PI_OUTPUT);

    while(1){
        gpioWrite(outpin,PI_HIGH);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        gpioWrite(outpin,PI_LOW);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}