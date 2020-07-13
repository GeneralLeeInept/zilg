#include "vgfw.h"

#include "gli_file.h"
#include "zmachine.h"

#include <vector>

GliFileSystem gFileSystem;
ZMachine zm;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
    std::vector<uint8_t> story_data;
    gFileSystem.read_entire_file("//zork1.zip//DATA/ZORK1.DAT", story_data);
    zm.load(story_data);

    for (;;)
        zm.update();

    return 0;
}
