#include "settings.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "pb_common.h"

#include "FlashPROM.h"

#include "hardware/structs/rosc.h"

#include <stdio.h>
#include <memory>

struct SettingsFooter
{
    uint32_t dataSize;
    uint32_t dataCRC;
    uint32_t magic;
};

static const uint32_t FOOTER_MAGIC = 0x34325043; // CP24

static uint32_t crc32(const uint8_t *data, size_t length)
{
    static const uint32_t LUT[] =
    {
        0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
        0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
        0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
        0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
    };

    uint32_t state = 0xffffffff;

    const uint8_t *dataEnd = data + length;
    while (data < dataEnd)
    {
        uint8_t tableIndex;

        tableIndex = state ^ *data;
        state = *(LUT + (tableIndex & 0x0f)) ^ (state >> 4);
        tableIndex = state ^ (*data >> 4);
        state = *(LUT + (tableIndex & 0x0f)) ^ (state >> 4);
        ++data;
    }

    return ~state;
}

static bool loadSettingsInner(Settings& settings)
{
    SettingsFooter footer;
    EEPROM.get(EEPROM_SIZE_BYTES - sizeof(SettingsFooter), footer);

    if (footer.magic != FOOTER_MAGIC)
    {
        return false;
    }

    if (footer.dataSize + sizeof(SettingsFooter) > EEPROM_SIZE_BYTES)
    {
        return false;
    }

    const uint8_t* dataPtr = EEPROM.cache + EEPROM_SIZE_BYTES - sizeof(SettingsFooter) - footer.dataSize;

    if (crc32(dataPtr, footer.dataSize) != footer.dataCRC)
    {
        return false;
    }

    pb_istream_t inputStream = pb_istream_from_buffer(dataPtr, footer.dataSize);

    return pb_decode(&inputStream, Settings_fields, &settings);
}

LoadSettingsResult loadSettings(Settings& settings)
{
    if (!loadSettingsInner(settings))
    {
        settings = Settings_init_default;
        return LoadSettingsResult::DefaultInit;
    }

    return LoadSettingsResult::Loaded;
}

static void setHasFlags(const pb_msgdesc_t* fields, void* s)
{
    pb_field_iter_t iter;
    if (!pb_field_iter_begin(&iter, fields, s))
    {
        return;
    }
    
    do
    {
        // Not implemented for extension fields
        assert(PB_LTYPE(iter.type) != PB_LTYPE_EXTENSION);

        if (PB_HTYPE(iter.type) == PB_HTYPE_OPTIONAL && iter.pSize)
        {
            *reinterpret_cast<char*>(iter.pSize) = true;
        }

        if (PB_LTYPE(iter.type) == PB_LTYPE_SUBMESSAGE)
        {
            assert(iter.submsg_desc);
            assert(iter.pData);

            setHasFlags(iter.submsg_desc, iter.pData);
        }
    } while (pb_field_iter_next(&iter));
}

SaveSettingsResult saveSettings(Settings& settings)
{
    // Set all has_XXX flags to true, we want to save all fields.
    setHasFlags(Settings_fields, &settings);

    // Encode the data
    std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(Settings_size);
    uint8_t* bufferPtr = buffer.get();
    pb_ostream_t outputStream = pb_ostream_from_buffer(bufferPtr, Settings_size);
    if (!pb_encode(&outputStream, Settings_fields, &settings))
    {
        return SaveSettingsResult::EncodingFailed;
    }

    // Write the footer
    const uint32_t dataCRC = crc32(bufferPtr, outputStream.bytes_written);
    SettingsFooter footer = {outputStream.bytes_written, dataCRC, FOOTER_MAGIC};
    EEPROM.set(EEPROM_SIZE_BYTES - sizeof(SettingsFooter), footer);

    // Write the data
    uint8_t* cachePtr = EEPROM.cache + EEPROM_SIZE_BYTES - sizeof(SettingsFooter) - outputStream.bytes_written;
    memcpy(cachePtr, bufferPtr, outputStream.bytes_written);

    EEPROM.commit();

    return SaveSettingsResult::Saved;
}

static void printIndentation(int level)
{
	static const char INDENTATION[] = "                ";
	while (level > 0)
	{
		if (level > 4)
		{
			printf(INDENTATION);
			level -= 4;
		}
		else
		{
			printf(INDENTATION + (4 - level) * 4);
			level = 0;
		}
	}
}

// Very rudimentary conversion to JSON

#define PREPROCESSOR_JOIN2(x, y) x ## y
#define PREPROCESSOR_JOIN(x, y) PREPROCESSOR_JOIN2(x, y)

#define PRINT_UENUM(fieldname, submessageType) printf("\"" #fieldname "\": %d", s.fieldname);
#define PRINT_INT32(fieldname, submessageType) printf("\"" #fieldname "\": %d", s.fieldname);
#define PRINT_UINT32(fieldname, submessageType) printf("\"" #fieldname "\": %d", s.fieldname);
#define PRINT_BOOL(fieldname, submessageType) printf("\"" #fieldname "\": %s", s.fieldname ? "true" : "false");
#define PRINT_MESSAGE(fieldname, submessageType) printf("\"" #fieldname "\":\n"); PREPROCESSOR_JOIN(dump, submessageType)(s.fieldname, indentationLevel + 1);

#define PRINT_FIELD(parenttype, atype, htype, ltype, fieldname, tag) \
	if (!firstField) printf(",\n"); \
	firstField = false; \
	printIndentation(indentationLevel + 1); \
	PREPROCESSOR_JOIN(PRINT_, ltype)(fieldname, parenttype ## _ ## fieldname ## _MSGTYPE)

#define GEN_DUMP_FUNCTION(structtype) \
	static void dump ## structtype(const structtype& s, int indentationLevel) \
	{ \
		printIndentation(indentationLevel); \
		printf("{\n"); \
		bool firstField = true; \
		structtype ## _FIELDLIST(PRINT_FIELD, structtype) \
        printf("\n"); \
		printIndentation(indentationLevel); \
		printf("}"); \
        if (indentationLevel == 0) printf("\n"); \
	}

GEN_DUMP_FUNCTION(GamepadOptions)
#if defined(Settings_boardOptions_MSGTYPE)
    GEN_DUMP_FUNCTION(BoardOptions)
#endif
#if defined(Settings_buzzerAddon_MSGTYPE)
    GEN_DUMP_FUNCTION(BuzzerAddon)
#endif
GEN_DUMP_FUNCTION(Settings)

void dumpSettings(const Settings& settings)
{
    dumpSettings(settings, 0);
}

// Random modifications to simulate a user changing values

class KissRNG
{
public:
    KissRNG() :
        x(generateSeedValue()),
        y(362436000),
        z(521288629),
        c(7654321)
    {}

    uint32_t next()
    {
        x = 69069 * x + 12345;
        y ^= y << 13;
        y ^= y >> 17;
        y ^= y << 5;
        uint64_t t = static_cast<uint64_t>(698769069) * z + c;
        c = t >> 32;
        z = static_cast<uint32_t>(t);
        return x + y + z;
    }

private:
    static uint32_t generateSeedValue()
    {
        uint32_t seedValue = 0;
        for (int i = 0; i < 32; ++i)
        {
            seedValue = (seedValue << 1) | rosc_hw->randombit;
        }
        return seedValue;
    }

    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t c;
};

static KissRNG rng;

#define MOD_UENUM(fieldname, submessageType, enumType) if (rng.next() % 2 == 0) { s.fieldname = static_cast<enumType>(rng.next() % 3); }
#define MOD_INT32(fieldname, submessageType, enumType) if (rng.next() % 2 == 0) { s.fieldname = rng.next() % 64; }
#define MOD_UINT32(fieldname, submessageType, enumType) if (rng.next() % 2 == 0) { s.fieldname = rng.next() % 64; }
#define MOD_BOOL(fieldname, submessageType, enumType) if (rng.next() % 2 == 0) { s.fieldname = !!(rng.next() % 2); }
#define MOD_MESSAGE(fieldname, submessageType, enumType) PREPROCESSOR_JOIN(mod, submessageType)(s.fieldname);

#define MOD_FIELD(parenttype, atype, htype, ltype, fieldname, tag) \
	PREPROCESSOR_JOIN(MOD_, ltype)(fieldname, parenttype ## _ ## fieldname ## _MSGTYPE, parenttype ## _ ## fieldname ## _ENUMTYPE)

#define GEN_MOD_FUNCTION(structtype) \
	static void mod ## structtype(structtype& s) { structtype ## _FIELDLIST(MOD_FIELD, structtype) }

GEN_MOD_FUNCTION(GamepadOptions)
#if defined(Settings_boardOptions_MSGTYPE)
    GEN_MOD_FUNCTION(BoardOptions)
#endif
#if defined(Settings_buzzerAddon_MSGTYPE)
    GEN_MOD_FUNCTION(BuzzerAddon)
#endif
GEN_MOD_FUNCTION(Settings)

void performRandomModifications(Settings& settings)
{
    modSettings(settings);
}
