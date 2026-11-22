#ifndef UTILS_H_
#define UTILS_H_

#define read_u32_be(buf) (unsigned int)(((buf)[0] << 24) + ((buf)[1] << 16) + ((buf)[2] << 8) + ((buf)[3]))
#define read_u16_be(buf) (((buf)[0] << 8) + ((buf)[1]))

int read_s16_be(unsigned char* buf);
float read_f32_be(unsigned char* buf);

#endif // UTILS_H_
