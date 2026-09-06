#ifndef LA_TX_PACK_H
#define LA_TX_PACK_H

/*
 * P4.9 TxTask burst transfer packer (pure logic).
 *
 * Turns one finished burst capture (an AcqTask JOB: total 32-bit words plus
 * the encoding and sample rate recorded at arm) into the ordered
 * protocol.md burst frames: DATA_BEGIN, DATA_CHUNK* (up to 1016 data bytes
 * each, payload offset headers in bytes), DATA_END. Chunk bytes are copied
 * from the capture buffer through a caller read callback, so the packer
 * owns no buffer memory. The DATA_END CRC32/IEEE is streamed over freshly
 * emitted chunk bytes and therefore equals the CRC of the whole capture
 * byte range (initial 0xFFFFFFFF, reflected; la_crc32_ieee).
 *
 * Boundaries (task wiring, P4.10): the packer does not assign frame
 * sequence numbers, does not track the ACK window (la_tx_win) and does not
 * encode frames (la_proto_encode). TxTask calls la_tx_pack_next() only when
 * its window can accept the produced frame and transmits every produced
 * frame exactly once, in order; a chunk retransmission is rebuilt
 * deterministically with la_tx_pack_chunk_at(), which reproduces the same
 * payload without advancing the offset or CRC state. The capture buffer
 * stays owned by the job until the END frame is on the wire and every
 * outstanding chunk has been resolved (window drained or burst abandoned);
 * calling la_tx_pack_next() after completion returns -1.
 *
 * Chunk boundaries always start at multiples of 1016 bytes. 1016 is
 * divisible by 4, so with whole words (raw packed or transition words are 4
 * bytes each) a 32-bit word is never split across chunks and every offset
 * stays word-aligned. Empty jobs (zero words) emit DATA_BEGIN followed by
 * DATA_END with the empty-buffer CRC.
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_TXPACK_API __declspec(dllexport)
#else
#define LA_TXPACK_API
#endif

typedef enum la_tx_pack_kind {
    LA_TX_PACK_BEGIN = 0, /* payload: DATA_BEGIN wire payload */
    LA_TX_PACK_CHUNK,     /* payload: DATA_CHUNK wire payload */
    LA_TX_PACK_END        /* payload: DATA_END wire payload */
} la_tx_pack_kind_t;

typedef struct la_tx_pack_out {
    la_tx_pack_kind_t kind;
    uint64_t byte_offset; /* CHUNK: first capture byte covered */
    size_t data_length;   /* CHUNK: data bytes behind the offset field */
} la_tx_pack_out_t;

/* Reads data_length bytes at byte_offset from the capture source; must
 * return a pointer to data_length readable bytes or NULL when the range is
 * not available. The pointer is consumed immediately (copied or hashed
 * before next() returns). */
typedef const uint8_t *(*la_tx_pack_read_fn)(void *context, uint64_t byte_offset);

typedef struct la_tx_pack {
    la_tx_pack_kind_t next_kind; /* kind the next next() call produces */
    uint64_t bytes_total;        /* capture bytes: total_words * 4 */
    uint64_t next_offset;        /* first byte of the next fresh chunk */
    uint32_t crc;                /* running CRC over fresh chunk bytes */
    uint32_t sample_rate_hz;
    uint8_t encoding;
    uint8_t finished;            /* END was produced; next() stays -1 */
} la_tx_pack_t;

/* Starts a burst job. Returns 0, or -1 when the capture cannot be addressed
 * by the protocol byte-offset field (total_bytes > UINT32_MAX) or on NULL. */
LA_TXPACK_API int la_tx_pack_init(la_tx_pack_t *pack,
                                  uint64_t total_words,
                                  uint8_t encoding,
                                  uint32_t sample_rate_hz);

/* Produces the next frame payload in order: DATA_BEGIN, DATA_CHUNK*,
 * DATA_END. Returns 0 with the payload in *payload and the metadata in
 * *out, or -1 when the job is complete, on invalid arguments or when a
 * chunk read callback fails (state unchanged on failure). */
LA_TXPACK_API int la_tx_pack_next(la_tx_pack_t *pack,
                                  la_tx_pack_read_fn read,
                                  void *read_context,
                                  uint8_t *payload,
                                  size_t capacity,
                                  la_tx_pack_out_t *out);

/* Deterministic rebuild of chunk index i for a retransmission: produces the
 * identical payload the first emission delivered, without touching the CRC
 * or offset state. Returns 0, or -1 when the chunk index is out of range or
 * the arguments are invalid. */
LA_TXPACK_API int la_tx_pack_chunk_at(const la_tx_pack_t *pack,
                                      uint64_t chunk_index,
                                      la_tx_pack_read_fn read,
                                      void *read_context,
                                      uint8_t *payload,
                                      size_t capacity,
                                      la_tx_pack_out_t *out);

#endif /* LA_TX_PACK_H */
