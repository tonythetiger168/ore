
// stratum_v2_client.c -- SKELETON (UNVERIFIED). Implements SV2 framing +
// message dispatch for the mining SoC. See sv2_protocol.md.
//
// TODO before use:
//  [ ] Noise handshake (libnoise or hand-rolled NK with secp256k1 keys)
//  [ ] official sv2-spec test vectors pass
//  [ ] integrate accel_* register layer from stratum_client.c
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

// ---- framing --------------------------------------------------------------
#define SV2_HDR 6
static void sv2_frame(uint8_t *out, uint16_t type, uint32_t payload_len) {
    out[0] = (uint8_t)(payload_len);
    out[1] = (uint8_t)(payload_len >> 8);
    out[2] = (uint8_t)(payload_len >> 16);
    out[3] = (uint8_t)(type);
    out[4] = (uint8_t)(type >> 8);
    out[5] = 0; // upper 4 bits reserved
}
static uint32_t sv2_payload_len(const uint8_t *h) {
    return (uint32_t)h[0] | ((uint32_t)h[1] << 8) | ((uint32_t)h[2] << 16);
}
static uint16_t sv2_msg_type(const uint8_t *h) {
    return (uint16_t)h[3] | ((uint16_t)(h[4] & 0x0F) << 8);
}

// ---- LE readers/writers (SV2 wire order) ----------------------------------
static void put_u8(uint8_t **p, uint8_t v) { *(*p)++ = v; }
static void put_u16le(uint8_t **p, uint16_t v) { put_u8(p, v); put_u8(p, v >> 8); }
static void put_u24le(uint8_t **p, uint32_t v) { put_u8(p, v); put_u8(p, v >> 8); put_u8(p, v >> 16); }
static void put_u32le(uint8_t **p, uint32_t v) { put_u8(p,v); put_u8(p,v>>8); put_u8(p,v>>16); put_u8(p,v>>24); }
static void put_bytes(uint8_t **p, const uint8_t *b, uint32_t n) { memcpy(*p, b, n); *p += n; }
static uint8_t get_u8(const uint8_t **p) { return *(*p)++; }
static void put_u64le(uint8_t **p, uint64_t v) { for (int i = 0; i < 8; i++) put_u8(p, (uint8_t)(v >> (8*i))); }
static uint64_t get_u64le(const uint8_t **p) { uint64_t v = 0; for (int i = 0; i < 8; i++) v |= (uint64_t)get_u8(p) << (8*i); return v; }
static uint16_t get_u16le(const uint8_t **p) { uint16_t v = get_u8(p); return v | ((uint16_t)get_u8(p) << 8); }
static uint32_t get_u32le(const uint8_t **p) { uint32_t v = 0; for (int i = 0; i < 4; i++) v |= (uint32_t)get_u8(p) << (8*i); return v; }
static void get_bytes(const uint8_t **p, uint8_t *b, uint32_t n) { memcpy(b, *p, n); *p += n; }

// ---- message types (mining protocol subset) --------------------------------
#define MT_SETUP_CONNECTION            0x0000
#define MT_SETUP_CONNECTION_SUCCESS    0x0001
#define MT_SETUP_CONNECTION_ERROR      0x0002
#define MT_NEW_TEMPLATE                0x0051
#define MT_SET_NEW_PREV_HASH           0x0052
#define MT_SUBMIT_SHARES               0x0069
#define MT_SUBMIT_SHARES_SUCCESS       0x006C
#define MT_SUBMIT_SHARES_ERROR         0x006D

// ---- pending template state -------------------------------------------------
typedef struct {
    bool     valid;
    uint64_t template_id;
    uint8_t  coinbase_prefix[256]; uint32_t prefix_len;
    uint8_t  coinbase_outputs[512]; uint32_t outputs_len;
    uint32_t locktime, input_sequence;
    uint8_t  merkle_path[10][32]; uint32_t path_len; // node count
} sv2_template_t;

static sv2_template_t g_tpl;
static uint32_t g_seq = 0, g_channel = 0;

// ---- handlers (wire crypto + accel glue left as stubs) ----------------------
static int on_new_template(const uint8_t *pl, uint32_t n) {
    const uint8_t *p = pl;
    g_tpl.template_id  = get_u64le(&p);          // needs get_u64le -- see TODO list
    g_tpl.input_sequence = get_u32le(&p);
    g_tpl.prefix_len   = get_u32le(&p); get_bytes(&p, g_tpl.coinbase_prefix, g_tpl.prefix_len);
    g_tpl.outputs_len  = get_u32le(&p); get_bytes(&p, g_tpl.coinbase_outputs, g_tpl.outputs_len);
    g_tpl.locktime     = get_u32le(&p);
    g_tpl.path_len     = get_u32le(&p);
    for (uint32_t i = 0; i < g_tpl.path_len && i < 10; i++) get_bytes(&p, g_tpl.merkle_path[i], 32);
    g_tpl.valid = true;
    return 0;
}
static int on_set_new_prev_hash(const uint8_t *pl, uint32_t n) {
    const uint8_t *p = pl;
    uint64_t tpl_id  = get_u64le(&p);
    uint8_t  prev[32], ntime_le[4], nbits_le[4], target[32];
    get_bytes(&p, prev, 32);
    get_bytes(&p, ntime_le, 4);
    get_bytes(&p, nbits_le, 4);
    get_bytes(&p, target, 32);
    (void)tpl_id; (void)ntime_le; (void)nbits_le;
    // TODO: assemble header(version from template flags), build coinbase =
    // prefix + our extranonce, dsha, fold merkle_path, then accel_load().
    // prev_hash is in Block1 -> midstate must be recomputed (full reload).
    (void)prev;
    return 0;
}
static int build_submit_shares(uint8_t *out, uint32_t job_id, uint32_t nonce,
                               uint32_t ntime, uint32_t version) {
    uint8_t *p = out + SV2_HDR;
    put_u32le(&p, g_channel);
    put_u32le(&p, ++g_seq);
    put_u32le(&p, job_id);
    put_u32le(&p, nonce);
    put_u32le(&p, ntime);
    put_u32le(&p, version);
    sv2_frame(out, MT_SUBMIT_SHARES, (uint32_t)(p - out - SV2_HDR));
    return (int)(p - out);
}

// ---- dispatch ---------------------------------------------------------------
static int dispatch(const uint8_t *frame) {
    uint16_t t = sv2_msg_type(frame);
    const uint8_t *pl = frame + SV2_HDR;
    uint32_t n = sv2_payload_len(frame);
    switch (t) {
        case MT_NEW_TEMPLATE:      return on_new_template(pl, n);
        case MT_SET_NEW_PREV_HASH: return on_set_new_prev_hash(pl, n);
        case MT_SUBMIT_SHARES_SUCCESS: return 0; // TODO accounting
        case MT_SUBMIT_SHARES_ERROR:   return 0; // TODO log/reconnect policy
        default: return -1;
    }
}
// NOTE: Noise transport is a stub (sv2_noise_stub_*). The message layer
// compiles standalone; official sv2-spec test vectors gate production use.

// ---- Noise transport stubs (replace with real NK handshake) -----------------
static int sv2_noise_stub_handshake(int fd, const uint8_t *pk_pool, const uint8_t *sk_self) {
    (void)fd; (void)pk_pool; (void)sk_self; return 0;
}
static int sv2_noise_stub_encrypt(uint8_t *buf, uint32_t n) { (void)buf; (void)n; return 0; }
static int sv2_noise_stub_decrypt(uint8_t *buf, uint32_t n) { (void)buf; (void)n; return 0; }
