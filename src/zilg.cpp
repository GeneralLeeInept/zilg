#include "vgfw.h"

#include "gli_file.h"
#include "log.h"
#include "zmachine.h"

#include <unordered_map>
#include <vector>
#include <memory>


GliFileSystem gFileSystem;


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
    std::vector<uint8_t> story_data;
    gFileSystem.read_entire_file("//zork_1_demo.zip//zork_1_demo.z3", story_data);
    return 0;
}
