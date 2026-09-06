#include "la_relay.h"
#include "la_proto.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                 \
    do {                                          \
        if (!(condition)) {                       \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                             \
        }                                         \
    } while (0)

typedef struct mock_ring {
    size_t free_bytes;
} mock_ring_t;

static int ring_capacity(void *context, size_t frame_bytes)
{
    mock_ring_t *ring = (mock_ring_t *)context;

    return frame_bytes <= ring->free_bytes ? 1 : 0;
}

static int init_zynq_leg(la_relay_t *relay)
{
    static const uint8_t best_effort[] = {LA_PROTO_EVT_DECODED};
    static const uint8_t mirror[] = {LA_PROTO_EVT_DECODED};

    return la_relay_init(relay, LA_RELAY_ROLE_UART_TO_NET, best_effort,
                         sizeof(best_effort), mirror, sizeof(mirror));
}

static int test_init_and_classification(void)
{
    la_relay_t relay;
    la_relay_stats_t stats;

    CHECK(init_zynq_leg(&relay) == 0, "Zynq leg init");
    CHECK(la_relay_is_best_effort(&relay, LA_PROTO_EVT_DECODED) == 1,
          "EVT_DECODED is best effort on the Zynq leg");
    CHECK(la_relay_is_best_effort(&relay, LA_PROTO_STATUS) == 0,
          "STATUS is control on the Zynq leg");
    CHECK(la_relay_is_best_effort(&relay, LA_PROTO_ACK) == 0,
          "ACK is control on the Zynq leg");
    CHECK(la_relay_is_best_effort(&relay, 0x03u) == 0,
          "unknown parseable types stay control (verbatim proxy)");
    CHECK(la_relay_is_mirror(&relay, LA_PROTO_EVT_DECODED) == 1,
          "EVT_DECODED is mirrored");
    CHECK(la_relay_is_mirror(&relay, LA_PROTO_LOG) == 0,
          "LOG is not mirrored");

    la_relay_snapshot(&relay, &stats);
    CHECK(stats.frames_forwarded == 0u && stats.best_effort_dropped == 0u &&
              stats.control_pressure_events == 0u,
          "fresh counters are zero");
    return 0;
}

static int test_forward_and_mirror(void)
{
    la_relay_t relay;
    mock_ring_t ring;
    la_relay_stats_t stats;
    la_relay_decision_t decision;

    CHECK(init_zynq_leg(&relay) == 0, "Zynq leg init");
    ring.free_bytes = 1000u;
    CHECK(la_relay_accept(&relay, LA_PROTO_EVT_DECODED, 120u, ring_capacity, &ring,
                          &decision) == 0 &&
              decision == LA_RELAY_FORWARD,
          "decoded frame forwards with space");
    CHECK(la_relay_report(&relay, LA_PROTO_EVT_DECODED, LA_RELAY_FWD_OK) == 0,
          "forward outcome reported");
    CHECK(la_relay_report(&relay, LA_PROTO_EVT_DECODED, LA_RELAY_MIRROR_OK) == 0,
          "mirror outcome reported");
    CHECK(la_relay_report(&relay, LA_PROTO_EVT_DECODED, LA_RELAY_MIRROR_FAIL) == 0,
          "mirror failure reported and counted");
    CHECK(la_relay_report(&relay, LA_PROTO_STATUS, LA_RELAY_MIRROR_OK) == -1,
          "mirror report on a non-mirror type rejected");

    CHECK(la_relay_accept(&relay, LA_PROTO_STATUS, 59u, ring_capacity, &ring,
                          &decision) == 0 &&
              decision == LA_RELAY_FORWARD,
          "control frame forwards with space");
    CHECK(la_relay_report(&relay, LA_PROTO_STATUS, LA_RELAY_FWD_OK) == 0,
          "control forward reported");
    CHECK(la_relay_report(&relay, LA_PROTO_STATUS, LA_RELAY_FWD_FAIL) == 0,
          "control failure reported and counted");

    la_relay_snapshot(&relay, &stats);
    CHECK(stats.frames_forwarded == 2u, "forward counter");
    CHECK(stats.frames_forward_failed == 1u, "forward failure counter");
    CHECK(stats.frames_mirrored == 1u, "mirror counter");
    CHECK(stats.mirror_dropped == 1u, "mirror drop counter");
    la_relay_reset_counters(&relay);
    la_relay_snapshot(&relay, &stats);
    CHECK(stats.frames_forwarded == 0u && stats.mirror_dropped == 0u,
          "counter reset keeps the configuration");
    CHECK(la_relay_is_best_effort(&relay, LA_PROTO_EVT_DECODED) == 1,
          "configuration survives the counter reset");
    return 0;
}

static int test_congestion_policy(void)
{
    la_relay_t relay;
    mock_ring_t ring;
    la_relay_stats_t stats;
    la_relay_decision_t decision;

    CHECK(init_zynq_leg(&relay) == 0, "Zynq leg init");
    ring.free_bytes = 100u;
    CHECK(la_relay_accept(&relay, LA_PROTO_EVT_DECODED, 200u, ring_capacity, &ring,
                          &decision) == 0 &&
              decision == LA_RELAY_DROP_BEST_EFFORT,
          "decoded frame drops newest when the ring is full");
    CHECK(la_relay_accept(&relay, LA_PROTO_STATUS, 200u, ring_capacity, &ring,
                          &decision) == 0 &&
              decision == LA_RELAY_PRESSURE,
          "control frame is preserved: pressure when the ring is full");
    CHECK(la_relay_accept(&relay, LA_PROTO_STATUS, 200u, ring_capacity, &ring,
                          &decision) == 0 &&
              decision == LA_RELAY_PRESSURE,
          "pressure counts every no-room attempt");
    ring.free_bytes = 300u;
    CHECK(la_relay_accept(&relay, LA_PROTO_STATUS, 200u, ring_capacity, &ring,
                          &decision) == 0 &&
              decision == LA_RELAY_FORWARD,
          "control frame forwards once the ring drains");
    CHECK(la_relay_report(&relay, LA_PROTO_STATUS, LA_RELAY_FWD_OK) == 0,
          "pressure-retried frame reported");
    la_relay_snapshot(&relay, &stats);
    CHECK(stats.best_effort_dropped == 1u, "drop counter");
    CHECK(stats.control_pressure_events == 2u, "pressure counter");
    CHECK(stats.frames_forwarded == 1u, "only the finally-placed frame counts");
    return 0;
}

static int test_unknown_types_never_dropped(void)
{
    la_relay_t relay;
    mock_ring_t ring;
    la_relay_stats_t stats;
    la_relay_decision_t decision;

    CHECK(init_zynq_leg(&relay) == 0, "Zynq leg init");
    ring.free_bytes = 0u;
    CHECK(la_relay_accept(&relay, 0x7Eu, 50u, ring_capacity, &ring, &decision) == 0 &&
              decision == LA_RELAY_PRESSURE,
          "unknown parseable type is control and never dropped by policy");
    ring.free_bytes = 50u;
    CHECK(la_relay_accept(&relay, 0x7Eu, 50u, ring_capacity, &ring, &decision) == 0 &&
              decision == LA_RELAY_FORWARD,
          "unknown type forwards verbatim once there is room");
    CHECK(la_relay_report(&relay, 0x7Eu, LA_RELAY_FWD_OK) == 0, "unknown type reported");
    la_relay_snapshot(&relay, &stats);
    CHECK(stats.frames_forwarded == 1u && stats.best_effort_dropped == 0u,
          "no best-effort drops for unknown types");
    return 0;
}

static int test_host_leg_all_control(void)
{
    la_relay_t relay;
    mock_ring_t ring;
    la_relay_stats_t stats;
    la_relay_decision_t decision;
    uint8_t type;

    CHECK(la_relay_init(&relay, LA_RELAY_ROLE_NET_TO_UART, NULL, 0u, NULL, 0u) == 0,
          "host leg init with empty tables");
    ring.free_bytes = 0u;
    CHECK(la_relay_is_best_effort(&relay, LA_PROTO_EVT_DECODED) == 0,
          "no best-effort class on the host leg");
    CHECK(la_relay_is_mirror(&relay, LA_PROTO_EVT_DECODED) == 0,
          "no mirror class on the host leg");
    for (type = 0x01u; type <= 0x03u; ++type) {
        CHECK(la_relay_accept(&relay, type, 20u, ring_capacity, &ring, &decision) == 0 &&
                  decision == LA_RELAY_PRESSURE,
              "host leg commands are never dropped by policy");
    }
    ring.free_bytes = 20u;
    CHECK(la_relay_accept(&relay, LA_PROTO_ARM, 20u, ring_capacity, &ring, &decision) == 0 &&
              decision == LA_RELAY_FORWARD,
          "host command forwards to the UART leg with space");
    CHECK(la_relay_report(&relay, LA_PROTO_ARM, LA_RELAY_FWD_OK) == 0, "host command reported");
    la_relay_snapshot(&relay, &stats);
    CHECK(stats.control_pressure_events == 3u && stats.frames_forwarded == 1u,
          "host leg pressure and forward counters");
    return 0;
}

static int test_argument_guards(void)
{
    la_relay_t relay;
    mock_ring_t ring;
    la_relay_decision_t decision;
    la_relay_stats_t stats;
    static const uint8_t too_many[LA_RELAY_MAX_TYPES + 1u] = {0u};

    CHECK(init_zynq_leg(&relay) == 0, "Zynq leg init");
    CHECK(la_relay_init(NULL, LA_RELAY_ROLE_UART_TO_NET, NULL, 0u, NULL, 0u) == -1,
          "NULL relay rejected");
    CHECK(la_relay_init(&relay, (la_relay_role_t)9u, NULL, 0u, NULL, 0u) == -1,
          "invalid role rejected");
    CHECK(la_relay_init(&relay, LA_RELAY_ROLE_UART_TO_NET, too_many,
                        sizeof(too_many), NULL, 0u) == -1,
          "oversized type table rejected");
    CHECK(la_relay_init(&relay, LA_RELAY_ROLE_UART_TO_NET, NULL, 1u, NULL, 0u) == -1,
          "NULL table with a count rejected");
    CHECK(la_relay_is_best_effort(NULL, LA_PROTO_EVT_DECODED) == 0,
          "classification on NULL relay reads as control");
    ring.free_bytes = 100u;
    CHECK(la_relay_accept(&relay, LA_PROTO_STATUS, 20u, NULL, &ring, &decision) == -1,
          "NULL capacity callback rejected");
    CHECK(la_relay_accept(&relay, LA_PROTO_STATUS, 20u, ring_capacity, &ring, NULL) == -1,
          "NULL decision pointer rejected");
    CHECK(la_relay_accept(&relay, LA_PROTO_STATUS, 0u, ring_capacity, &ring, &decision) == -1,
          "zero frame bytes rejected");
    CHECK(la_relay_report(NULL, LA_PROTO_STATUS, LA_RELAY_FWD_OK) == -1,
          "NULL relay report rejected");
    CHECK(la_relay_report(&relay, LA_PROTO_STATUS, (la_relay_result_t)99u) == -1,
          "invalid result rejected");
    CHECK(la_relay_report(&relay, LA_PROTO_STATUS, LA_RELAY_MIRROR_OK) == -1,
          "mirror result on non-mirror type rejected");
    la_relay_snapshot(NULL, &stats);
    la_relay_snapshot(&relay, NULL);
    return 0;
}

int main(void)
{
    if (test_init_and_classification() != 0 || test_forward_and_mirror() != 0 ||
        test_congestion_policy() != 0 || test_unknown_types_never_dropped() != 0 ||
        test_host_leg_all_control() != 0 || test_argument_guards() != 0) {
        return 1;
    }
    puts("PASS: la_esp32_mocks_relay");
    return 0;
}
