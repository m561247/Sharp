//
// Created by BNunnally on 9/14/2020.
//

#ifndef SHARP_SERIALIZATION_H
#define SHARP_SERIALIZATION_H

#include "../../runtime/memory/sharp_object.h"

#define BUFFER_ALLOC_CHUNK_SIZE_STANDARD KB_TO_BYTES(24)
#define BUFFER_ALLOC_CHUNK_SIZE_LARGE MB_TO_BYTES(2)
#define SERIALIZE_START (0x3b)
#define SERIALIZE_END (0x3d)
#define REFERENCE_OBJECT (0xf)
#define DATA_BEGIN (0xe)
#define DATA_END (0x2e)
#define CLASS_SECTION_BEGIN (0x002)
#define CLASS_SECTION_END (0x1c)
#define NULL_OBJECT (0x008)
#define OBJECT_ID_START (0x0)
#define STANDARD_OBJECT (0x009)
#define NUMERIC_OBJECT (0x3a)
#define CLASS_OBJECT (0x1a)
#define CLASS_ARRAY_OBJECT (0x1b)

struct serialize_buffer_t
{
    char *buf = nullptr;
    uint32_t size = 0;
    uint32_t pos = -1;
};

struct deserialize_buffer_t
{
    long double *buf = nullptr;
    uint32_t size = 0;
    uint32_t pos = -1;
    sharp_object **references = nullptr;
};

struct serialized_classes_t
{
    sharp_class **serialized = nullptr;
    uint32_t size = 0;
    uint32_t count = 0;
};

struct object_buffer_t
{
    sharp_object **buf = nullptr;
    int32_t size = 0;
    int32_t pos = -1;
};

#define buffer_size(buf) \
buf.pos + 1

#define push_data(bufer, data) \
     if(((bufer.pos) + 1) >= (bufer.size)) { \
        alloc_buffer(bufer); \
    } \
    (bufer.buf)[++bufer.pos] = (data); 

#define push_int32(buffer, data) \
    if(((buffer.pos) + 4) >= (buffer.size)) { \
        alloc_buffer(buffer); \
    } \
(buffer.buf)[++(buffer.pos)] = GET_i32w(data); \
    (buffer.buf)[++(buffer.pos)] = GET_i32x(data); \
    (buffer.buf)[++(buffer.pos)] = GET_i32y(data); \
    (buffer.buf)[++(buffer.pos)] = GET_i32z(data);

#define push_bytes(buffer, type, data) \
    unsigned char *bytes = reinterpret_cast<unsigned char*>(&data); \
    for(int jj = 0; jj < sizeof(type); jj++) { \
        push_data(buffer, bytes[jj]) \
    }

#define formatted_buffer(pos) \
    ((uint8_t) dBuffer.buf[(pos)])

#define expect_data(data) \
    if((dBuffer.pos + 1) >= dBuffer.size) { \
        throw vm_exception("invalid format: unexpected end of deserialization buffer");\
    } else if(formatted_buffer(++dBuffer.pos) != data) { \
        throw vm_exception("unexpected data found in deserialization buffer");\
    }

#define overflow_check \
    if((dBuffer.pos + 1) >= dBuffer.size) { \
        throw vm_exception("invalid format: unexpected end of deserialization buffer");\
    }

#define read_int32(out) \
    if((dBuffer.pos + 4) >= dBuffer.size) { \
        throw vm_exception("invalid format: unexpected end of deserialization buffer");\
    } \
    (out) = SET_i32(formatted_buffer(dBuffer.pos+1), formatted_buffer(dBuffer.pos+2), \
                formatted_buffer(dBuffer.pos+3), formatted_buffer(dBuffer.pos+4)); \
    dBuffer.pos += 4;

#define read_data \
    formatted_buffer(++dBuffer.pos)

#define pek_data(out) \
    if((dBuffer.pos + 1) >= dBuffer.size) { \
        throw vm_exception("invalid format: unexpected end of deserialization buffer");\
    } \
    (out) = formatted_buffer(dBuffer.pos+1);

#define read_bytes(type, bytes, out) \
    if((dBuffer.pos + sizeof(type)) >= dBuffer.size) { \
        throw vm_exception("invalid format: unexpected end of deserialization buffer");\
    } \
    for(int jj = 0; jj < sizeof(type); jj++) { \
        bytes[jj] = read_data; \
    } \
    out = *reinterpret_cast<type*>(bytes); \

void serialize(object *from, object *to);
void deserialize(object *from, object *to);

#endif //SHARP_SERIALIZATION_H
