#include "zmachine.h"

#include "log.h"

#include <algorithm>


static const char* default_alphabet[3] = {
    "      abcdefghijklmnopqrstuvwxyz",
    "      ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "       \n0123456789.,!?_#'\"/\\-:()"
};

static const ZMachine::InstructionHandlers instruction_handlers_3 {
    { 0x01, &ZMachine::_je },
    { 0x02, &ZMachine::_jl },
    { 0x03, &ZMachine::_jg },
    { 0x04, &ZMachine::_dec_chk },
    { 0x05, &ZMachine::_inc_chk },
    { 0x06, &ZMachine::_jin },
    { 0x07, &ZMachine::_test },
    { 0x08, &ZMachine::_or },
    { 0x09, &ZMachine::_and },
    { 0x0A, &ZMachine::_test_attr },
    { 0x0B, &ZMachine::_set_attr },
    { 0x0C, &ZMachine::_clear_attr },
    { 0x0D, &ZMachine::_store },
    { 0x0E, &ZMachine::_insert_obj },
    { 0x0F, &ZMachine::_loadw },
    { 0x10, &ZMachine::_loadb },
    { 0x11, &ZMachine::_get_prop },
    { 0x12, &ZMachine::_get_prop_addr },
    //{ 0x13, &ZMachine::_get_next_prop },
    { 0x14, &ZMachine::_add }, { 0x15, &ZMachine::_sub },
    //{ 0x16, &ZMachine::_mul },
    //{ 0x17, &ZMachine::_div },
    //{ 0x18, &ZMachine::_mod },
    { 0x80, &ZMachine::_jz },
    { 0x81, &ZMachine::_get_sibling },
    { 0x82, &ZMachine::_get_child },
    { 0x83, &ZMachine::_get_parent },
    { 0x84, &ZMachine::_get_prop_len },
    //{ 0x85, &ZMachine::_inc },
    //{ 0x86, &ZMachine::_dec },
    //{ 0x87, &ZMachine::_print_addr },
    //{ 0x89, &ZMachine::_remove_obj },
    { 0x8A, &ZMachine::_print_obj },
    { 0x8B, &ZMachine::_ret }, { 0x8C, &ZMachine::_jump },
    { 0x8D, &ZMachine::_print_paddr },
    //{ 0x8E, &ZMachine::_load },
    //{ 0x8F, &ZMachine::_not },
    { 0xB0, &ZMachine::_rtrue },
    { 0xB1, &ZMachine::_rfalse },
    { 0xB2, &ZMachine::_print },
    //{ 0xB3, &ZMachine::_print_ret },
    //{ 0xB4, &ZMachine::_nop },
    //{ 0xB5, &ZMachine::_save },
    //{ 0xB6, &ZMachine::_restore },
    //{ 0xB7, &ZMachine::_restart },
    { 0xB8, &ZMachine::_ret_popped },
    { 0xB9, &ZMachine::_pop },
    //{ 0xBA, &ZMachine::_quit },
    { 0xBB, &ZMachine::_new_line },
    //{ 0xBC, &ZMachine::_show_status },
    //{ 0xBD, &ZMachine::_verify },
    { 0xE0, &ZMachine::_call }, { 0xE1, &ZMachine::_storew },
    //{ 0xE2, &ZMachine::_storeb },
    { 0xE3, &ZMachine::_put_prop },
    { 0xE4, &ZMachine::_sread },
    { 0xE5, &ZMachine::_print_char },
    { 0xE6, &ZMachine::_print_num },
    //{ 0xE7, &ZMachine::_random },
    { 0xE8, &ZMachine::_push },
    { 0xE9, &ZMachine::_pull },
    //{ 0xEA, &ZMachine::_split_window },
    //{ 0xEB, &ZMachine::_set_window },
    //{ 0xF3, &ZMachine::_output_stream },
    //{ 0xF4, &ZMachine::_input_stream },
};


static const ZMachine::ObjectTraits object_traits_3 {
    1, // Size of an object index in bytes
    9, // Size of an object in bytes
    4, // Number of bytes of attribute flags
    31 // Maximum number of object properties
};


static const ZMachine::Traits traits_3 {
    instruction_handlers_3,
    2,      // Function address scale
    object_traits_3
};


static void swap_endian(ZMachineHeader& header)
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


bool ZMachine::load(std::vector<uint8_t>& story_file)
{
    size_t size = std::min(story_file.size(), size_t(64 * 1024));
    memcpy(_memory, story_file.data(), size);

    uint8_t version = story_file[0];

    if (version == 3)
    {
        _traits = traits_3;
    }
    else
    {
        logf("Unsupported zmachine version %d\n", version);
        return false;
    }

    // TODO - validate

    reset();

    return true;
}


void ZMachine::reset()
{
    memcpy(&_header, _memory, sizeof(ZMachineHeader));
    swap_endian(_header);

    // TODO: Flags

    _pc = _header.initial_pc;
    _sp = 0xFFFF;
    _locals_base = _sp;
}


void ZMachine::update()
{
    ZInstruction instruction{};
    uint8_t opcode = read(_pc++);

    if (opcode == 0xBE)
    {
        // TODO: EXTOP
        // TODO: Traps / exceptions
    }
    else if (opcode < 0x80)
    {
        // 0x00..0x7F - 2OP  (0 m m o o o o o)
        instruction.opcode = opcode & 0x1F;
        instruction.operand_count = 2;
        instruction.operand_types[0] = 1 + ((opcode >> 6) & 1);
        instruction.operand_types[1] = 1 + ((opcode >> 5) & 1);
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

        uint8_t operands = read(_pc++);

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

        uint8_t num_operand_bytes = (opcode == 0xEC || opcode == 0xFA) ? 2 : 1;

        for (uint8_t i = 0; i < num_operand_bytes; ++i)
        {
            uint8_t operands = read(_pc++);

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

        if (instruction.operand_types[i] == OpImm16)
        {
            uint8_t msb = read(_pc++);
            uint8_t lsb = read(_pc++);
            operand = make_word(msb, lsb);
        }
        else if (instruction.operand_types[i] == OpImm8)
        {
            operand = read(_pc++);
        }
        else
        {
            uint8_t var = read(_pc++);
            operand = readv(var);
        }

        instruction.operands[i] = operand;
    }

    InstructionHandlers::iterator& it = _traits.instruction_handlers.find(instruction.opcode);

    if (it == _traits.instruction_handlers.end())
    {
        logf("Illegal opcode %04X\n", instruction.opcode);
    }
    else
    {
        InstructionHandler handler = it->second;
        (this->*handler)(instruction);
    }
}


uint8_t ZMachine::read(uint16_t addr)
{
    return _memory[addr];
}


void ZMachine::write(uint16_t addr, uint8_t byte)
{
    _memory[addr] = byte;
}


uint16_t ZMachine::readw(uint16_t addr)
{
    uint8_t msb = _memory[addr];
    uint8_t lsb = _memory[addr + 1];
    return make_word(msb, lsb);
}


void ZMachine::writew(uint16_t addr, uint16_t word)
{
    _memory[addr] = hi(word);
    _memory[addr + 1] = lo(word);
}


uint8_t ZMachine::read_table(uint16_t addr, uint16_t index)
{
    return read(addr + index);
}


void ZMachine::write_table(uint16_t addr, uint16_t index, uint8_t byte)
{
    write(addr + index, byte);
}


uint16_t ZMachine::read_tablew(uint16_t addr, uint16_t index)
{
    return readw(addr + (index << 1));
}


void ZMachine::write_tablew(uint16_t addr, uint16_t index, uint16_t word)
{
    writew(addr + (index << 1), word);
}


uint16_t ZMachine::read_string(uint16_t addr, std::vector<char>& str, bool terminate)
{
    uint16_t triplet = 0;
    uint16_t len = 0;
    uint8_t decoder_mode = 0;
    uint16_t zscii_code = 0;

    /*
        decoder modes:
        0,1,2 - read from alphabet n
        3,4,5 - abbreviation
        6     - raw ZSCII high
        7     - raw ZSCII low
    */

    for(; (triplet & 0x8000) == 0;)
    {
        triplet = readw(addr);
        addr += 2;
        len += 2;

        // Unpack triple
        for (uint8_t i = 15; i > 0;)
        {
            i -= 5;
            char c = (triplet >> i) & 0x1F;

            if (decoder_mode < 3)
            {
                if (c >= 1 && c < 4)
                {
                    decoder_mode = 2 + c;
                }
                else if (c >= 4 && c < 6)
                {
                    decoder_mode = c - 3;
                }
                else if (decoder_mode == 2 && c == 6)
                {
                    decoder_mode = 6;
                }
                else
                {
                    str.push_back(c ? default_alphabet[decoder_mode][c] : ' ');
                    decoder_mode = 0;
                }
            }
            else if (decoder_mode < 6)
            {
                uint8_t index = ((decoder_mode - 3) << 5) | c;
                uint16_t abbreviation = read_tablew(_header.abbreviations_table, index) << 1;
                read_string(abbreviation, str, false);
                decoder_mode = 0;
            }
            else if (decoder_mode == 6)
            {
                zscii_code = (c & 0x1F) << 5;
                decoder_mode = 7;
            }
            else if (decoder_mode == 7)
            {
                zscii_code = zscii_code | (c & 0x1F);
                decoder_mode = 0;

                if (zscii_to_ascii(c, zscii_code, true))
                {
                    str.push_back(c);
                }

                decoder_mode = 0;
            }
        }
    }

    if (terminate)
    {
        str.push_back(0);
    }

    return len;
}


uint16_t ZMachine::readv(uint8_t var)
{
    uint16_t value = 0;

    if (var == 0)
    {
        // stack top
        value = pop();
    }
    else if (var < 16)
    {
        // local variable
        value = _stack[_locals_base - var];
    }
    else
    {
        // global variable
        uint8_t index = var - 16;
        value = read_tablew(_header.globals_table, index);
    }

    return value;
}


void ZMachine::writev(uint8_t var, uint16_t value)
{
    if (var == 0)
    {
        push(value);
    }
    else if (var < 16)
    {
        _stack[_locals_base - var] = value;
    }
    else
    {
        uint8_t index = var - 16;
        write_tablew(_header.globals_table, index, value);
    }
}


void ZMachine::push(uint16_t value)
{
    // TODO: Stack overflow
    _stack[--_sp] = value;
}


uint16_t ZMachine::pop()
{
    // TODO: Stack underflow
    return _stack[_sp++];
}


void ZMachine::push_stack_frame()
{
    push(_pc);
    push(_locals_base);
    _locals_base = _sp;
}


void ZMachine::pop_stack_frame()
{
    _sp = _locals_base;
    _locals_base = pop();
    _pc = pop();
}


uint16_t ZMachine::get_object_ptr(uint16_t object_index)
{
    // TODO: Validate object_index < 2^object_traits.object_index_size_bytes
    uint16_t object_base = _header.object_table + (_traits.object_traits.max_properties << 1);
    uint16_t object_ptr = object_base + (object_index - 1) * _traits.object_traits.object_size_bytes;
    return object_ptr;
}


uint8_t ZMachine::get_attribute(uint16_t object_index, uint8_t attribute_index)
{
    // TODO: Validate that flag < object_traits.attribute_flag_bytes * 8
    uint16_t object_ptr = get_object_ptr(object_index);
    uint8_t byte_index = (attribute_index >> 3);
    uint8_t attribute_bit = 0x80 >> (attribute_index & 0x7);
    uint8_t attributes = read(object_ptr + byte_index);
    return (attributes & attribute_bit);
}


void ZMachine::set_attribute(uint16_t object_index, uint8_t attribute_index, uint8_t value)
{
    // TODO: Validate that flag < object_traits.attribute_flag_bytes * 8
    uint16_t object_ptr = get_object_ptr(object_index);
    uint8_t byte_index = (attribute_index >> 3);
    uint8_t attribute_bit = 0x80 >> (attribute_index & 0x7);
    uint8_t attributes = read(object_ptr + byte_index);
    attributes = value ? (attributes | attribute_bit) : (attributes & (~attribute_bit));
    write(object_ptr + byte_index, attributes);
}


uint16_t ZMachine::get_parent(uint16_t object_index)
{
    // TODO: Validate that object_index is in range
    uint16_t object_ptr = get_object_ptr(object_index);
    uint16_t parent_ptr = object_ptr + _traits.object_traits.attribute_flag_bytes;
    return (_traits.object_traits.object_index_size_bytes == 1) ? read(parent_ptr) : readw(parent_ptr);
}


void ZMachine::set_parent(uint16_t object_index, uint16_t parent_index)
{
    // TODO: Validate that object indices are in range
    uint16_t object_ptr = get_object_ptr(object_index);
    uint16_t parent_ptr = object_ptr + _traits.object_traits.attribute_flag_bytes;
    (_traits.object_traits.object_index_size_bytes == 1) ? write(parent_ptr, (uint8_t)parent_index) : writew(parent_ptr, parent_index);
}


uint16_t ZMachine::get_sibling(uint16_t object_index)
{
    // TODO: Validate that object_index is in range
    uint16_t object_ptr = get_object_ptr(object_index);
    uint16_t sibling_ptr = object_ptr + _traits.object_traits.attribute_flag_bytes + _traits.object_traits.object_index_size_bytes;
    return (_traits.object_traits.object_index_size_bytes == 1) ? read(sibling_ptr) : readw(sibling_ptr);
}


void ZMachine::set_sibling(uint16_t object_index, uint16_t sibling_index)
{
    // TODO: Validate that object indices are in range
    uint16_t object_ptr = get_object_ptr(object_index);
    uint16_t sibling_ptr = object_ptr + _traits.object_traits.attribute_flag_bytes + _traits.object_traits.object_index_size_bytes;
    (_traits.object_traits.object_index_size_bytes == 1) ? write(sibling_ptr, (uint8_t)sibling_index) : writew(sibling_ptr, sibling_index);
}


uint16_t ZMachine::get_child(uint16_t object_index)
{
    // TODO: Validate that object_index is in range
    uint16_t object_ptr = get_object_ptr(object_index);
    uint16_t child_ptr = object_ptr + _traits.object_traits.attribute_flag_bytes + _traits.object_traits.object_index_size_bytes * 2;
    return (_traits.object_traits.object_index_size_bytes == 1) ? read(child_ptr) : readw(child_ptr);
}


void ZMachine::set_child(uint16_t object_index, uint16_t child_index)
{
    // TODO: Validate that object indices are in range
    uint16_t object_ptr = get_object_ptr(object_index);
    uint16_t child_ptr = object_ptr + _traits.object_traits.attribute_flag_bytes + _traits.object_traits.object_index_size_bytes * 2;
    (_traits.object_traits.object_index_size_bytes == 1) ? write(child_ptr, (uint8_t)child_index) : writew(child_ptr, child_index);
}


void ZMachine::get_object_short_name(uint16_t object_index, std::vector<char>& str)
{
    uint16_t object_ptr = get_object_ptr(object_index);
    uint16_t property_ptr = readw(object_ptr + _traits.object_traits.attribute_flag_bytes + _traits.object_traits.object_index_size_bytes * 3);
    uint8_t short_name_len = read(property_ptr++);

    if (short_name_len)
    {
        read_string(property_ptr, str, true);
    }
    else
    {
        str.push_back(0);
    }
}

bool ZMachine::get_prop_addr(uint16_t object_index, uint8_t property_index, uint16_t& prop_addr, uint8_t& prop_size)
{
    // TODO: Validate indices
    uint16_t object_ptr = get_object_ptr(object_index);
    uint16_t property_ptr = readw(object_ptr + _traits.object_traits.attribute_flag_bytes + _traits.object_traits.object_index_size_bytes * 3);
    uint8_t header_size = read(property_ptr++);
    property_ptr += (header_size << 1);

    uint8_t property_size = 0;
    prop_addr = 0;
    prop_size = 0;

    for (uint8_t property_number = _traits.object_traits.max_properties; property_number > property_index; property_ptr += property_size)
    {
        uint8_t size_byte = read(property_ptr++);

        if (size_byte == 0)
        {
            break;
        }

        // TODO: Could this be done in traits (without getting templates involved)?
        if (_header.version < 4 || (size_byte & 0x80) == 0)
        {
            property_number = size_byte & 0x1F;
            property_size = (size_byte >> 5) + 1;
        }
        else
        {
            property_number = size_byte & 0x3F;
            property_size = read(property_ptr++) & 0x3F;
        }

        if (property_number == property_index)
        {
            prop_addr = property_ptr;
            prop_size = prop_size;
        }
    }

    return !!prop_addr;
}


uint8_t ZMachine::get_prop_len(uint16_t prop_addr)
{
    uint8_t size_byte = read(prop_addr - 1);

    if (_header.version > 4 || (size_byte & 0x80) == 0)
    {
        return (size_byte >> 5) + 1;
    }
    else
    {
        return read(prop_addr - 2) & 0x3F;
    }
}


uint16_t ZMachine::get_prop(uint16_t object_index, uint8_t property_index)
{
    uint16_t prop_addr;
    uint8_t prop_size;
    uint16_t value;

    if (get_prop_addr(object_index, property_index, prop_addr, prop_size))
    {
        // TODO: Validate prop_size
        if (prop_size == 1)
        {
            value = read(prop_addr);
        }
        else
        {
            value = readw(prop_addr);
        }
    }
    else
    {
        value = read_tablew(_header.object_table, property_index);
    }

    return value;
}


void ZMachine::put_prop(uint16_t object_index, uint8_t property_index, uint16_t value)
{
    uint16_t prop_addr;
    uint8_t prop_size;

    if (get_prop_addr(object_index, property_index, prop_addr, prop_size))
    {
        if (prop_size == 1)
        {
            write(prop_addr, (uint8_t)value);
        }
        else
        {
            // TODO: Validate prop_size
            writew(prop_addr, value);
        }
    }
    else
    {
        // TODO: HCF
        std::vector<char> short_name;
        short_name.reserve(766);
        get_object_short_name(object_index, short_name);
        logf("Illegal property access: obj %X [%s], prop %X\n", object_index, short_name, property_index);
    }
}


void ZMachine::dump_objects()
{
    logm("\n\nDumping object tree:\n");
    logm("--------------------\n");
    for (uint16_t obj = 1; obj < 256; ++obj)
    {
        if (get_parent(obj) == 0)
        {
            dump_object(obj, 0);
        }
    }
    logm("--------------------\n");
}


void ZMachine::dump_object(uint16_t obj, uint8_t level)
{
    std::vector<char> desc;
    desc.reserve(756);

    while (obj)
    {
        desc.resize(0);
        get_object_short_name(obj, desc);
        logf("%02x: %*s%s\n", obj, level * 2, "", desc.data());

        uint16_t child = get_child(obj);

        if (child)
        {
            dump_object(child, level + 1);
        }

        obj = get_sibling(obj);
    }
}


void ZMachine::store_result(uint16_t value)
{
    uint8_t var = read(_pc++);
    writev(var, value);
}


void ZMachine::apply_predicate(bool test)
{
    uint8_t predicate = read(_pc++);
    uint16_t offset;

    if (predicate & 0x40)
    {
        offset = predicate & 0x3F;
    }
    else
    {
        uint8_t sign_extend = (predicate & 0x20) ? 0xC0 : 0x00;
        uint8_t msb = sign_extend | (predicate & 0x3F);
        uint8_t lsb = read(_pc++);
        offset = make_word(msb, lsb);
    }

    bool polarity = !!(predicate & 0x80);

    if (test == polarity)
    {
        if (offset > 1)
        {
            int16_t loc = offset;
            _pc = _pc + loc - 2;
        }
        else
        {
            ret(offset);
        }
    }
}


void ZMachine::ret(uint16_t result)
{
    pop_stack_frame();
    store_result(result);
}


bool ZMachine::zscii_to_ascii(char& ascii_code, uint16_t zscii_code, bool for_output)
{
    bool skip = true;

    if (for_output)
    {
        if (zscii_code == 13)
        {
            ascii_code = '\n';
            skip = false;
        }
        else if (zscii_code >= 32 && zscii_code < 127)
        {
            ascii_code = (char)zscii_code;
            skip = false;
        }
    }

    return !skip;
}


void ZMachine::_je(ZInstruction& instruction)
{
    bool equal = true;

    for (uint8_t i = 1; equal && i < instruction.operand_count; ++i)
    {
        equal = equal && instruction.operands[0] == instruction.operands[i];
    }

    apply_predicate(equal);
}


void ZMachine::_jl(ZInstruction& instruction)
{
    int16_t a = (int16_t)instruction.operands[0];
    int16_t b = (int16_t)instruction.operands[1];
    bool less = a < b;
    apply_predicate(less);
}


void ZMachine::_jg(ZInstruction& instruction)
{
    int16_t a = (int16_t)instruction.operands[0];
    int16_t b = (int16_t)instruction.operands[1];
    bool greater = a > b;
    apply_predicate(greater);
}


void ZMachine::_dec_chk(ZInstruction& instruction)
{
    uint8_t var = (uint8_t)instruction.operands[0];
    uint16_t value = readv(var);
    value--;
    writev(var, value);
    bool less = value < instruction.operands[1];
    apply_predicate(less);
}


void ZMachine::_inc_chk(ZInstruction& instruction)
{
    uint8_t var = (uint8_t)instruction.operands[0];
    uint16_t value = readv(var);
    value++;
    writev(var, value);
    bool greater = value > instruction.operands[1];
    apply_predicate(greater);
}


void ZMachine::_jin(ZInstruction& instruction)
{
    uint16_t obj_a = instruction.operands[0];
    uint16_t obj_b = instruction.operands[1];
    bool in = get_parent(obj_a) == obj_b;
    apply_predicate(in);
}


void ZMachine::_test(ZInstruction& instruction)
{
    uint16_t bitmap = instruction.operands[0];
    uint16_t flags = instruction.operands[1];
    bool test = (bitmap & flags) == flags;
    apply_predicate(test);
}


void ZMachine::_or(ZInstruction& instruction)
{
    uint16_t a = instruction.operands[0];
    uint16_t b = instruction.operands[1];
    uint16_t result = a | b;
    store_result(result);
}


void ZMachine::_and(ZInstruction& instruction)
{
    uint16_t a = instruction.operands[0];
    uint16_t b = instruction.operands[1];
    uint16_t result = a & b;
    store_result(result);
}


void ZMachine::_test_attr(ZInstruction& instruction)
{
    uint16_t object_index = instruction.operands[0];
    uint8_t attribute_index = (uint8_t)instruction.operands[1];
    uint8_t attribute = get_attribute(object_index, attribute_index);
    apply_predicate(!!attribute);
}


void ZMachine::_set_attr(ZInstruction& instruction)
{
    uint16_t object_index = instruction.operands[0];
    uint8_t attribute_index = (uint8_t)instruction.operands[1];
    set_attribute(object_index, attribute_index, 1);
}


void ZMachine::_clear_attr(ZInstruction& instruction)
{
    uint16_t object_index = instruction.operands[0];
    uint8_t attribute_index = (uint8_t)instruction.operands[1];
    set_attribute(object_index, attribute_index, 0);
}


void ZMachine::_store(ZInstruction& instruction)
{
    uint8_t var = (uint8_t)instruction.operands[0];
    uint16_t value = instruction.operands[1];
    writev(var, value);
}


void ZMachine::_insert_obj(ZInstruction& instruction)
{
    uint16_t object_index = instruction.operands[0];

    // Unlink object from current parent and siblings
    uint16_t parent_index = get_parent(object_index);

    if (parent_index)
    {
        uint16_t sibling_index = get_sibling(object_index);
        uint16_t prev_index = 0;

        for (uint16_t child_index = get_child(parent_index); child_index != object_index; child_index = get_sibling(child_index))
        {
            prev_index = child_index; 
        }

        if (prev_index)
        {
            set_sibling(prev_index, sibling_index);
        }
        else
        {
            set_child(parent_index, sibling_index);
        }

        set_sibling(object_index, 0);
    }

    // Add as first child of new parent
    parent_index = instruction.operands[1];
    set_parent(object_index, parent_index);

    if (parent_index)
    {
        uint16_t sibling_index = get_child(parent_index);
        set_sibling(object_index, sibling_index);
        set_child(parent_index, object_index);
    }
}


void ZMachine::_loadw(ZInstruction& instruction)
{
    uint16_t table = instruction.operands[0];
    uint16_t index = instruction.operands[1];
    uint16_t value = read_tablew(table, index);
    store_result(value);
}


void ZMachine::_loadb(ZInstruction& instruction)
{
    uint16_t table = instruction.operands[0];
    uint16_t index = instruction.operands[1];
    uint8_t value = read_table(table, index);
    store_result(value);
}


void ZMachine::_get_prop(ZInstruction& instruction)
{
    uint16_t object_index = instruction.operands[0];
    uint8_t property_index = (uint8_t)instruction.operands[1];
    uint16_t value = get_prop(object_index, property_index);
    store_result(value);
}


void ZMachine::_get_prop_addr(ZInstruction& instruction)
{
    uint16_t object_index = instruction.operands[0];
    uint8_t property_index = (uint8_t)instruction.operands[1];
    uint16_t addr;
    uint8_t size;
    get_prop_addr(object_index, property_index, addr, size);
    store_result(addr);
}


void ZMachine::_get_next_prop(ZInstruction& instruction)
{

}


void ZMachine::_add(ZInstruction& instruction)
{
    uint16_t result = instruction.operands[0] + instruction.operands[1];
    store_result(result);
}


void ZMachine::_sub(ZInstruction& instruction)
{
    uint16_t result = instruction.operands[0] - instruction.operands[1];
    store_result(result);
}


void ZMachine::_mul(ZInstruction& instruction) {}
void ZMachine::_div(ZInstruction& instruction) {}
void ZMachine::_mod(ZInstruction& instruction) {}
void ZMachine::_call_2s_4(ZInstruction& instruction) {}
void ZMachine::_call_2n_5(ZInstruction& instruction) {}
void ZMachine::_set_colour_5(ZInstruction& instruction) {}
void ZMachine::_set_colour_6(ZInstruction& instruction) {}
void ZMachine::_throw_5(ZInstruction& instruction) {}


void ZMachine::_jz(ZInstruction& instruction)
{
    bool zero = instruction.operands[0] == 0;
    apply_predicate(zero);
}


void ZMachine::_get_sibling(ZInstruction& instruction)
{
    uint16_t object_index = instruction.operands[0];
    uint16_t sibling_index = get_sibling(object_index);
    store_result(sibling_index);
    apply_predicate(!!sibling_index);
}


void ZMachine::_get_child(ZInstruction& instruction)
{
    uint16_t object_index = instruction.operands[0];
    uint16_t child_index = get_child(object_index);
    store_result(child_index);
    apply_predicate(!!child_index);
}


void ZMachine::_get_parent(ZInstruction& instruction)
{
    uint16_t object_index = instruction.operands[0];
    uint16_t parent_index = get_parent(object_index);
    store_result(parent_index);
}


void ZMachine::_get_prop_len(ZInstruction& instruction)
{
    uint16_t prop_addr = instruction.operands[0];
    uint16_t prop_len = 0;

    if (prop_addr)
    {
        prop_len = get_prop_len(prop_addr);
    }

    store_result(prop_len);
}


void ZMachine::_inc(ZInstruction& instruction) {}
void ZMachine::_dec(ZInstruction& instruction) {}
void ZMachine::_print_addr(ZInstruction& instruction) {}
void ZMachine::_call_1s_4(ZInstruction& instruction) {}
void ZMachine::_remove_obj(ZInstruction& instruction) {}


void ZMachine::_print_obj(ZInstruction& instruction)
{
    uint16_t object_index = instruction.operands[0];
    std::vector<char> short_name;
    short_name.reserve(756);
    get_object_short_name(object_index, short_name);
    logm(short_name.data());
}


void ZMachine::_ret(ZInstruction& instruction)
{
    ret(instruction.operands[0]);
}


void ZMachine::_jump(ZInstruction& instruction)
{
    uint16_t loc = instruction.operands[0];
    _pc = _pc + loc - 2;
}


void ZMachine::_print_paddr(ZInstruction& instruction)
{
    uint16_t paddr = instruction.operands[0];
    std::vector<char> str(128);
    // TODO: paddr unpacking (traits)
    read_string(paddr << 2, str, true);
    logm(str.data());
}


void ZMachine::_load(ZInstruction& instruction) {}
void ZMachine::_not(ZInstruction& instruction) {}
void ZMachine::_call_1n_5(ZInstruction& instruction) {}

void ZMachine::_rtrue(ZInstruction& instruction)
{
    ret(1);
}


void ZMachine::_rfalse(ZInstruction& instruction)
{
    ret(0);
}


void ZMachine::_print(ZInstruction& instruction)
{
    std::vector<char> str;
    str.reserve(128);
    uint16_t literal_length = read_string(_pc, str, true);
    _pc += literal_length;
    logm(str.data());
}


void ZMachine::_print_ret(ZInstruction& instruction) {}
void ZMachine::_nop(ZInstruction& instruction) {}
void ZMachine::_save(ZInstruction& instruction) {}
void ZMachine::_save_4(ZInstruction& instruction) {}
void ZMachine::_restore(ZInstruction& instruction) {}
void ZMachine::_restore_4(ZInstruction& instruction) {}
void ZMachine::_restart(ZInstruction& instruction) {}


void ZMachine::_ret_popped(ZInstruction& instruction)
{
    uint16_t value = pop();
    ret(value);
}


void ZMachine::_pop(ZInstruction& instruction)
{
    pop();
}


void ZMachine::_catch_5(ZInstruction& instruction) {}
void ZMachine::_quit(ZInstruction& instruction) {}


void ZMachine::_new_line(ZInstruction& instruction)
{
    logm("\n");
}


void ZMachine::_show_status(ZInstruction& instruction) {}
void ZMachine::_verify(ZInstruction& instruction) {}
void ZMachine::_extended_5(ZInstruction& instruction) {}
void ZMachine::_piracy_5(ZInstruction& instruction) {}


void ZMachine::_call(ZInstruction& instruction)
{
    uint16_t fnc = instruction.operands[0] * _traits.function_address_scale;

    if (fnc == 0)
    {
        store_result(0);
        return;
    }

    push_stack_frame();
    _pc = fnc;

    uint8_t num_locals = read(_pc++);
    uint8_t num_args = instruction.operand_count - 1;

    for (uint8_t i = 0; i < num_locals; ++i)
    {
        uint16_t value = readw(_pc);
        _pc += 2;

        if (i < num_args)
        {
            value = instruction.operands[i + 1];
        }

        push(value);
    }
}


void ZMachine::_call_vs_4(ZInstruction& instruction) {}


void ZMachine::_storew(ZInstruction& instruction)
{
    uint16_t table = instruction.operands[0];
    uint8_t index = (uint8_t)instruction.operands[1];
    uint16_t value = instruction.operands[2];
    write_tablew(table, index, value);
}


void ZMachine::_storeb(ZInstruction& instruction) {}


void ZMachine::_put_prop(ZInstruction& instruction)
{
    uint16_t object_index = instruction.operands[0];
    uint8_t property_index = (uint8_t)instruction.operands[1];
    uint16_t value = instruction.operands[2];
    put_prop(object_index, property_index, value);
}


void ZMachine::_sread(ZInstruction& instruction)
{
    dump_objects();
    for(;;);
}


void ZMachine::_sread_4(ZInstruction& instruction) {}
void ZMachine::_aread_5(ZInstruction& instruction) {}


void ZMachine::_print_char(ZInstruction& instruction)
{
    uint16_t zscii = instruction.operands[0];
    char ascii;

    if (zscii_to_ascii(ascii, zscii, true))
    {
        logf("%c", ascii);
    }
}


void ZMachine::_print_num(ZInstruction& instruction)
{
    int16_t number = instruction.operands[0];
    logf("%d", number);
}


void ZMachine::_random(ZInstruction& instruction) {}


void ZMachine::_push(ZInstruction& instruction)
{
    push(instruction.operands[0]);
}


void ZMachine::_pull(ZInstruction& instruction)
{
    uint8_t var = (uint8_t)instruction.operands[0];
    uint16_t value = pop();
    writev(var, value);
}


void ZMachine::_pull_6(ZInstruction& instruction) {}
void ZMachine::_split_window(ZInstruction& instruction) {}
void ZMachine::_set_window(ZInstruction& instruction) {}
void ZMachine::_call_vs2_4(ZInstruction& instruction) {}
void ZMachine::_erase_window_4(ZInstruction& instruction) {}
void ZMachine::_erase_line_4(ZInstruction& instruction) {}
void ZMachine::_erase_line_6(ZInstruction& instruction) {}
void ZMachine::_set_cursor_4(ZInstruction& instruction) {}
void ZMachine::_set_cursor_6(ZInstruction& instruction) {}
void ZMachine::_get_cursor_4(ZInstruction& instruction) {}
void ZMachine::_set_text_style_4(ZInstruction& instruction) {}
void ZMachine::_buffer_mode_4(ZInstruction& instruction) {}
void ZMachine::_output_stream(ZInstruction& instruction) {}
void ZMachine::_output_stream_5(ZInstruction& instruction) {}
void ZMachine::_output_stream_6(ZInstruction& instruction) {}
void ZMachine::_input_stream(ZInstruction& instruction) {}
void ZMachine::_sound_effect_5(ZInstruction& instruction) {}
void ZMachine::_read_char_4(ZInstruction& instruction) {}
void ZMachine::_scan_table_4(ZInstruction& instruction) {}
void ZMachine::_not_5(ZInstruction& instruction) {}
void ZMachine::_call_vn_5(ZInstruction& instruction) {}
void ZMachine::_call_vn2_5(ZInstruction& instruction) {}
void ZMachine::_tokenise_5(ZInstruction& instruction) {}
void ZMachine::_encode_text_5(ZInstruction& instruction) {}
void ZMachine::_copy_table_5(ZInstruction& instruction) {}
void ZMachine::_print_table_5(ZInstruction& instruction) {}
void ZMachine::_check_arg_count_5(ZInstruction& instruction) {}