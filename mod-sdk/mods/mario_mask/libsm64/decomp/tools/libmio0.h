#ifndef LIBMIO0_H_
#define LIBMIO0_H_

// defines

#define MIO0_HEADER_LENGTH 16

// typedefs

typedef struct {
    unsigned int dest_size;
    unsigned int comp_offset;
    unsigned int uncomp_offset;
} mio0_header_t;

// function prototypes

// decode MIO0 header
// returns 1 if valid header, 0 otherwise
int mio0_decode_header(const unsigned char* buf, mio0_header_t* head);

// decode MIO0 data in memory
// in: buffer containing MIO0 data
// out: buffer for output data
// end: output offset of the last byte decoded from in (set to NULL if unwanted)
// returns bytes extracted to 'out' or negative value on failure
int mio0_decode(const unsigned char* in, unsigned char* out, unsigned int* end);

#endif // LIBMIO0_H_
