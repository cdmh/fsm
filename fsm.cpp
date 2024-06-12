#include "pedestrian_crossing.h"

namespace {

void run()
{
    pedestrian_crossing::crossing_state_machine crossing;
    crossing.set_event(pedestrian_crossing::events::initialised());

    bool quit = false;
    while (!quit)
    {
        char ch;
        std::cin.get(ch);

        switch (ch)
        {
            
            case 'q':
            case 'Q':
                quit = true;
                break;

            case 'b':
            case 'B':
                crossing.set_event(pedestrian_crossing::events::press_button());
                break;
        }
    }
}

}   // anonymous namespace

int main()
{
    run();
}
