#pragma once
#include <cstddef>
#include <cstring>

// Read-only MSVC std::string ABI view. LoadSetting only calls c_str(); this
// object must never be passed to a host function that mutates or destroys it.
struct PresetStringView
{
    char storage[16] = {};
    std::size_t size = 0;
    std::size_t capacity = 15;

    const char *data() const
    {
        if (capacity <= 15) return storage;
        const char *pointer = nullptr;
        std::memcpy(&pointer, storage, sizeof(pointer));
        return pointer;
    }
};
static_assert(sizeof(PresetStringView) == 32);

inline bool valid_preset_string(const PresetStringView &value)
{
    return value.size != 0 && value.size <= value.capacity && value.data() != nullptr;
}

template <std::size_t N>
inline bool preset_string_equals(const PresetStringView &value, const char (&expected)[N])
{
    return valid_preset_string(value) && value.size == N - 1 &&
        std::memcmp(value.data(), expected, N - 1) == 0;
}

inline bool native_setting_uses_presets(const PresetStringView &key,
                                        const PresetStringView &section)
{
    return preset_string_equals(key, "DirectNeuralRenderingEncoding") ||
        preset_string_equals(section, "Neural Details");
}

inline bool make_preset_section(const PresetStringView &global_name, int preset,
                               char (&buffer)[128], PresetStringView &section)
{
    if (preset < 1 || preset > 3 || !valid_preset_string(global_name) ||
        global_name.size > sizeof(buffer) - 9)
        return false;
    const char *name = global_name.data();
    if (name == nullptr) return false;
    std::memcpy(buffer, name, global_name.size);
    std::memcpy(buffer + global_name.size, "-preset1", 9);
    buffer[global_name.size + 7] = static_cast<char>('0' + preset);
    section = {};
    section.size = global_name.size + 8;
    if (section.size <= 15)
        std::memcpy(section.storage, buffer, section.size + 1);
    else
    {
        const char *pointer = buffer;
        std::memcpy(section.storage, &pointer, sizeof(pointer));
        section.capacity = sizeof(buffer) - 1;
    }
    return true;
}

template <typename Read, typename Write>
inline unsigned seed_preset_setting(const PresetStringView &global_name,
                                    const PresetStringView &key,
                                    Read read, Write write)
{
    if (!valid_preset_string(global_name) || !valid_preset_string(key)) return 0;
    char value[128] = {};
    std::size_t value_size = sizeof(value);
    if (!read(global_name.data(), key.data(), value, &value_size) || value_size > sizeof(value)) return 0;
    value[sizeof(value) - 1] = 0;
    unsigned seeded = 0;
    for (int preset = 1; preset <= 3; ++preset)
    {
        char section_buffer[128] = {}, existing[128] = {};
        PresetStringView section;
        if (!make_preset_section(global_name, preset, section_buffer, section)) return seeded;
        std::size_t existing_size = sizeof(existing);
        if (!read(section_buffer, key.data(), existing, &existing_size))
        {
            write(section_buffer, key.data(), value);
            ++seeded;
        }
    }
    return seeded;
}
