#include "vgfw.h"

#include "gli_file.h"
#include "log.h"
#include "zmachine.h"

#include <deque>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>


GliFileSystem gFileSystem;


void swap_endian(ZStoryHeader& header)
{
    header.release_number = swap_endian(header.release_number);
    header.high_mem_base = swap_endian(header.high_mem_base);
    header.initial_pc = swap_endian(header.initial_pc);
    header.dictionary_table = swap_endian(header.dictionary_table);
    header.object_table = swap_endian(header.object_table);
    header.globals_table = swap_endian(header.globals_table);
    header.static_mem_base = swap_endian(header.static_mem_base);
    header.game_flags = swap_endian(header.game_flags);
    header.abbreviations_table = swap_endian(header.abbreviations_table);
    header.file_length = swap_endian(header.file_length);
    header.file_checksum = swap_endian(header.file_checksum);
    header.screen_width_units = swap_endian(header.screen_width_units);
    header.screen_height_units = swap_endian(header.screen_height_units);
    header.routines_offset = swap_endian(header.routines_offset);
    header.static_strings_offset = swap_endian(header.static_strings_offset);
    header.terminating_characters_table = swap_endian(header.terminating_characters_table);
    header.stream3_output_width = swap_endian(header.stream3_output_width);
    header.standard_revision_number = swap_endian(header.standard_revision_number);
    header.alphabet_table = swap_endian(header.alphabet_table);
    header.extension_table = swap_endian(header.extension_table);
}


void dump_header(ZStoryHeader& header)
{
    logf("Version: %d\n", header.version);
    logf("Interpreter flags: %X\n", header.interpreter_flags);
    logf("Release number: %d\n", header.release_number);
    logf("Preloaded code size: %X\n", header.high_mem_base);
    logf("Initial PC: %X\n", header.initial_pc);
    logf("Dictionary table: %X\n", header.dictionary_table);
    logf("Object table: %X\n", header.object_table);
    logf("Global variables table: %X\n", header.globals_table);
    logf("Dynamic memory size: %X\n", header.static_mem_base);
    logf("Game flags: %X\n", header.game_flags);
    logf("Serial number: %c%c%c%c%c%c\n", header.serial[0], header.serial[1], header.serial[2], header.serial[3], header.serial[4], header.serial[5]);
    logf("Abbreviations table: %X\n", header.abbreviations_table);
}


std::vector<uint8_t> story_data;
ZStoryHeader g_header{};

void disassemble(uint16_t start_address)
{
    struct ZInstruction
    {
        uint16_t pc;
        uint16_t opcode;
        uint8_t operand_count;
        uint8_t operand_types[8];
        uint16_t operands[8];
    };

    std::deque<uint16_t> address_queue;
    std::vector<ZInstruction> disassembly;
    std::map<uint16_t, size_t> ordered_disassembly;

    static const char* format_names[] = {
        "2OP",
        "1OP",
        "0OP",
        "EXT",
    };

    static const char* operand_typenames[] = { "{long immediate}", "{immediate}", "{variable}" };
    static const char* operand_formats[] = { " #$%04X", " #$%02X", " $%02X" };

    static std::unordered_map<uint16_t, std::string> opcode_names = {
        // 2-OP
        { 0x01, "je" },
        { 0x02, "jl" },
        { 0x03, "jg" },
        { 0x04, "dec_chk" },
        { 0x05, "inc_chk" },
        { 0x06, "jin" },
        { 0x07, "test" },
        { 0x08, "or" },
        { 0x09, "and" },
        { 0x0A, "test_attr" },
        { 0x0B, "set_attr" },
        { 0x0C, "clear_attr" },
        { 0x0D, "store" },
        { 0x0E, "insert_obj" },
        { 0x0F, "loadw" },
        { 0x10, "loadb" },
        { 0x11, "get_prop" },
        { 0x12, "get_prop_addr" },
        { 0x13, "get_next_prop" },
        { 0x14, "add" },
        { 0x15, "sub" },
        { 0x16, "mul" },
        { 0x17, "div" },
        { 0x18, "mod" },
        { 0x19, "call_2s" },
        { 0x1A, "call_2n" },
        { 0x1B, "set_colour" },
        { 0x1C, "throw" },

        // 1-OP
        { 0x80, "jz" },
        { 0x81, "get_sibling" },
        { 0x82, "get_child" },
        { 0x83, "get_parent" },
        { 0x84, "get_prop_len" },
        { 0x85, "inc" },
        { 0x86, "dec" },
        { 0x87, "print_addr" },
        { 0x88, "call_1s" },
        { 0x89, "remove_obj" },
        { 0x8A, "print_obj" },
        { 0x8B, "ret" },
        { 0x8C, "jump" },
        { 0x8D, "print_paddr" },
        { 0x8E, "load" },
        { 0x8F, "not" },

        // 0-OP
        { 0xB0, "rtrue" },
        { 0xB1, "rfalse" },
        { 0xB2, "print" },
        { 0xB3, "print_ret" },
        { 0xB4, "nop" },
        { 0xB5, "save" },
        { 0xB6, "restore" },
        { 0xB7, "restart" },
        { 0xB8, "ret_popped" },
        { 0xB9, "pop" },
        { 0xBA, "quit" },
        { 0xBB, "new_line" },
        { 0xBC, "show_status" },
        { 0xBD, "verify" },
        { 0xBE, "extended" },
        { 0xBF, "piracy" },

        // EXT
        { 0xE0, "call" },
        { 0xE1, "storew" },
        { 0xE2, "storeb" },
        { 0xE3, "put_prop" },
        { 0xE4, "sread" },
        { 0xE5, "print_char" },
        { 0xE6, "print_num" },
        { 0xE7, "random" },
        { 0xE8, "push" },
        { 0xE9, "pull" },
        { 0xEA, "split_window" },
        { 0xEB, "set_window" },
        { 0xEC, "call_vs2" },
        { 0xED, "erase_window" },
        { 0xEE, "erase_line" },
        { 0xEF, "set_cursor" },
        { 0xF0, "get_cursor" },
        { 0xF1, "set_text_style" },
        { 0xF2, "buffer_mode" },
        { 0xF3, "output_stream" },
        { 0xF4, "input_stream" },
        { 0xF5, "sound_effect" },
        { 0xF6, "read_char" },
        { 0xF7, "scan_table" },
        { 0xF8, "not" },
        { 0xF9, "call_vn" },
        { 0xFA, "call_vn2" },
        { 0xFB, "tokenise" },
        { 0xFC, "encode_text" },
        { 0xFD, "copy_table" },
        { 0xFE, "print_table" },
        { 0xFF, "check_arg_count" },
    };

    char buf[1024];

    address_queue.push_back(start_address);

    while (!address_queue.empty())
    {
        uint16_t address = address_queue.front();
        address_queue.pop_front();

        logf("Popped address %04X...\n", address);

        for (bool cont = true; cont;)
        {
            if (ordered_disassembly.find(address) != ordered_disassembly.end())
            {
                break;
            }

            /*
                4.3
                Each instruction has a form (long, short, extended or variable) and an operand count (0OP, 1OP, 2OP or VAR). If the top two bits of
                the opcode are $$11 the form is variable; if $$10, the form is short. If the opcode is 190 ($BE in hexadecimal) and the version is 5
                or later, the form is "extended". Otherwise, the form is "long".

                4.3.1
                In short form, bits 4 and 5 of the opcode byte give an operand type as above. If this is $11 then the operand count is 0OP;
                otherwise, 1OP. In either case the opcode number is given in the bottom 4 bits.

                4.3.2
                In long form the operand count is always 2OP. The opcode number is given in the bottom 5 bits.

                4.3.3
                In variable form, if bit 5 is 0 then the count is 2OP; if it is 1, then the count is VAR. The opcode number is given in the bottom 5
                bits.

                4.3.4
                In extended form, the operand count is VAR. The opcode number is given in a second opcode byte.
            */
            ZInstruction instruction{};
            uint16_t pc = address;
            uint8_t opcode = story_data[address++];

            logf("    Disassembling instruction %d @ %04X...\n", opcode, pc);

            if (opcode == 0xBE)
            {
                // TODO: EXTOP
            }
            else if (opcode < 0x80)
            {
                // 0x00..0x7F - 2OP  (0 m m o o o o o)
                instruction.opcode = opcode & 0x1F;
                instruction.operand_count = 2;
                instruction.operand_types[0] = (opcode >> 6) & 1;
                instruction.operand_types[1] = (opcode >> 5) & 1;
            }
            else if (opcode < 0xB0)
            {
                // 0x80..0xAF - 1OP (1 0 m m o o o o)
                instruction.opcode = 0x80 | opcode & 0xF;
                instruction.operand_count = 1;
                instruction.operand_types[0] = (opcode >> 4) & 0x3;
            }
            else if (opcode < 0xC0)
            {
                // 0xB0..0xBF - 0OP (1 0 1 1 o o o o)
                instruction.opcode = 0xB0 | opcode & 0xF;
                instruction.operand_count = 0;
            }
            else if (opcode < 0xE0)
            {
                // 0xC0..0xDF - EXT (1 1 0 o o o o o)
                // EXT versions of 2OP opcodes
                instruction.opcode = opcode & 0x1F;

                uint8_t operands = story_data[address++];

                for (int o = 0; (o < 4) && ((operands & 0xC0) != 0xC0); ++o)
                {
                    instruction.operand_types[instruction.operand_count] = operands >> 6;
                    instruction.operand_count++;
                    operands <<= 2;
                }
            }
            else
            {
                // 0xE0..0xFF - EXT (1 1 o o o o o o)
                instruction.opcode = 0xC0 | opcode & 0x3F;

                uint8_t num_operand_bytes = (opcode == 236 || opcode == 250) ? 2 : 1;

                for (uint8_t i = 0; i < num_operand_bytes; ++i)
                {
                    uint8_t operands = story_data[address++];

                    for (int o = 0; (o < 4) && ((operands & 0xC0) != 0xC0); ++o)
                    {
                        instruction.operand_types[instruction.operand_count] = operands >> 6;
                        instruction.operand_count++;
                        operands <<= 2;
                    }
                }
            }

            for (int i = 0; i < instruction.operand_count; ++i)
            {
                uint16_t operand = 0;

                if (instruction.operand_types[i] == 0)
                {
                    uint8_t msb = story_data[address++];
                    uint8_t lsb = story_data[address++];
                    operand = make_word(msb, lsb);
                }
                else
                {
                    operand = story_data[address++];
                }

                instruction.operands[i] = operand;
            }

            instruction.pc = pc;
            ordered_disassembly.emplace(pc, disassembly.size());
            disassembly.push_back(instruction);

            switch (instruction.opcode)
            {
                // branch with predicate
                case 0x01:
                case 0x02:
                case 0x03:
                case 0x04:
                case 0x05:
                case 0x06:
                case 0x07:
                case 0x0A:
                case 0x80:
                case 0x81:
                case 0x82:
                case 0xB5:
                case 0xB6:
                case 0xBD:
                case 0xBF:
                case 0xF7:
                {
                    uint8_t predicate = story_data[address++];
                    uint16_t offset;

                    if (predicate & 0x40)
                    {
                        offset = predicate & 0x3F;
                    }
                    else
                    {
                        uint8_t sign_extend = (predicate & 0x20) ? 0xC0 : 0x00;
                        uint8_t msb = sign_extend | (predicate & 0x3F);
                        uint8_t lsb = story_data[address++];
                        offset = make_word(msb, lsb);
                    }

                    if (offset > 1)
                    {
                        int16_t loc = offset;
                        uint16_t destination = address + loc - 2;
                        address_queue.push_back(destination);
                        logf("        (PRED) Pushed address %04X\n", destination);
                    }
                    else
                    {
                        // Skip store byte
                        ++address;
                    }

                    break;
                }

                // jump
                case 0x8C:
                {
                    int16_t loc = instruction.operands[0];
                    uint16_t destination = address + loc - 2;
                    address_queue.push_back(destination);
                    logf("        (JUMP) Pushed address %04X\n", destination);
                    cont = false;
                    break;
                }

                // call
                case 0x19:
                case 0x1A:
                case 0x88:
                case 0xE0:
                case 0xEC:
                {
                    // Skip return byte and fall through to ICALL flavors
                    ++address;
                }
                case 0xF9:
                case 0xFA:
                {
                    uint16_t destination = instruction.operands[0] << 1;

                    if (destination != 0)
                    {
                        destination += g_header.routines_offset << 3;

                        // Skip function prolog
                        uint8_t locals_count = story_data[destination++];
                        destination += locals_count << 1;

                        address_queue.push_back(destination);
                        logf("        (CALL) Pushed address %04X\n", destination);
                    }
                    break;
                }

                // these have an extra store byte operand
                case 0x08:
                case 0x09:
                case 0x0F:
                case 0x10:
                case 0x11:
                case 0x12:
                case 0x13:
                case 0x14:
                case 0x15:
                case 0x16:
                case 0x17:
                case 0x18:
                case 0x83:
                case 0x84:
                case 0x8E:
                case 0x8F:
                {
                    ++address;
                    break;
                }

                // PC won't advance past these ones...
                case 0x1C: // throw
                case 0x8B: // ret
                case 0xB0: // rtrue
                case 0xB1: // rfalse
                case 0xB8: // ret_popped
                case 0xB7: // restart
                case 0xBA: // quit
                {
                    cont = false;
                    break;
                }
            }
        }
    }

    for (const auto& instruction : disassembly)
    {
        int buflen = 0;

        auto it = opcode_names.find(instruction.opcode);

        if (it == opcode_names.end())
        {
            char newopcode[128];
            snprintf(newopcode, 128, "%0*X (???)", instruction.opcode > 255 ? 4 : 2, instruction.opcode);
            auto result = opcode_names.emplace(instruction.opcode, newopcode);
            it = result.first;
        }

        const char* opcode_name = it->second.c_str();
        buflen += snprintf(&buf[buflen], 1024 - buflen, "%04X: (%02X) %s", instruction.pc, story_data[instruction.pc], opcode_name);

        for (int i = 0; i < instruction.operand_count; ++i)
        {
            if (buflen <= 0)
            {
                break;
            }

            buflen += snprintf(&buf[buflen], 1024 - buflen, operand_formats[instruction.operand_types[i]], instruction.operands[i]);
        }

        logf("%s\n", buf);
    }
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
    gFileSystem.read_entire_file("//zork1.zip//DATA/ZORK1.DAT", story_data);
    memcpy(&g_header, story_data.data(), sizeof(ZStoryHeader));
    swap_endian(g_header);
    dump_header(g_header);
    disassemble(g_header.initial_pc);

    return 0;
}
