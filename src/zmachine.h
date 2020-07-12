#pragma once

#include <cstdint>

struct ZStoryHeader
{
    uint8_t version;
    uint8_t interpreter_flags;
    uint16_t release_number;
    uint16_t high_mem_base;
    uint16_t initial_pc;
    uint16_t dictionary_table;
    uint16_t object_table;
    uint16_t globals_table;
    uint16_t static_mem_base;
    uint16_t game_flags;
    uint8_t serial[6];
    uint16_t abbreviations_table;
    uint16_t file_length;
    uint16_t file_checksum;
    uint8_t interpreter_number;
    uint8_t interpreter_version;
    uint8_t screen_height_chars;
    uint8_t screen_width_chars;
    uint16_t screen_width_units;
    uint16_t screen_height_units;
    uint8_t font_width_units;
    uint8_t font_height_units;
    uint16_t routines_offset;
    uint16_t static_strings_offset;
    uint8_t default_background_color;
    uint8_t default_foreground_color;
    uint16_t terminating_characters_table;
    uint16_t stream3_output_width;
    uint16_t standard_revision_number;
    uint16_t alphabet_table;
    uint16_t extension_table;
};


inline uint16_t swap_endian(uint16_t x)
{
    return ((x & 0xFF00) >> 8) | ((x & 0x00FF) << 8);
}


inline uint8_t lo(uint16_t word)
{
    return (word & 0xFF);
}


inline uint8_t hi(uint16_t word)
{
    return (word >> 8);
}


inline uint16_t make_word(uint8_t hi, uint8_t lo)
{
    return (hi << 8) | lo;
}
