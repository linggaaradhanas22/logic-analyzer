#include "la_web_core.h"

#include <stdio.h>
#include <string.h>

static int append(la_web_core_t *core, const char *text)
{
    size_t need = strlen(text);

    if (core->length + need >= core->capacity) {
        core->truncated = 1u;
        return -1;
    }
    memcpy(core->buffer + core->length, text, need + 1u);
    core->length += need;
    return 0;
}

void la_web_core_init(la_web_core_t *core, char *buffer, size_t capacity)
{
    if (core == NULL) {
        return;
    }
    memset(core, 0, sizeof(*core));
    core->buffer = buffer;
    core->capacity = capacity;
    if (buffer != NULL && capacity > 0u) {
        buffer[0] = '\0';
    }
}

int la_web_core_begin(la_web_core_t *core, uint8_t slot)
{
    char head[24];

    if (core == NULL || core->buffer == NULL || slot >= LA_WEB_CORE_SLOTS) {
        return -1;
    }
    (void)snprintf(head, sizeof(head), "{\"s\":%u,\"e\":[", (unsigned)slot);
    if (append(core, head) != 0) {
        return -1;
    }
    core->slots[slot] += 1u;
    return 0;
}

int la_web_core_event(la_web_core_t *core, uint8_t kind, uint64_t t_sample,
                      const uint8_t *data, uint8_t length)
{
    static const char hex_digits[] = "0123456789abcdef";
    char item[40];
    size_t index;

    if (core == NULL || core->buffer == NULL) {
        return -1;
    }
    if (length > LA_WEB_CORE_MAX_DATA) {
        length = LA_WEB_CORE_MAX_DATA;
    }
    if (core->events > 0u && append(core, ",") != 0) {
        return -1;
    }
    (void)snprintf(item, sizeof(item), "[%llu,%u,\"",
                   (unsigned long long)t_sample, (unsigned)kind);
    if (append(core, item) != 0) {
        return -1;
    }
    for (index = 0u; index < (size_t)length; ++index) {
        char pair[3];
        uint8_t value = data != NULL ? data[index] : 0u;

        pair[0] = hex_digits[(value >> 4) & 0x0Fu];
        pair[1] = hex_digits[value & 0x0Fu];
        pair[2] = '\0';
        if (append(core, pair) != 0) {
            return -1;
        }
    }
    if (append(core, "\"]") != 0) {
        return -1;
    }
    core->events += 1u;
    return 0;
}

int la_web_core_end(la_web_core_t *core)
{
    if (core == NULL || core->buffer == NULL) {
        return -1;
    }
    if (append(core, "]}") != 0) {
        return -1;
    }
    return core->truncated != 0u ? -1 : 0;
}
