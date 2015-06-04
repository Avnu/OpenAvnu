#include "eui48.h"
#include "pdu.h"

void eui48_init(struct eui48 *self)
{
	int i;
	for (i = 0; i < sizeof(self->value); ++i) {
		self->value[i] = 0xff;
	}
}

void eui48_zero(struct eui48 *self)
{
	int i;
	for (i = 0; i < sizeof(self->value); ++i) {
		self->value[i] = 0x00;
	}
}

void eui48_init_from_uint64(struct eui48 *self, uint64_t other)
{
	self->value[0] = (uint8_t)((other >> (5 * 8)) & 0xff);
	self->value[1] = (uint8_t)((other >> (4 * 8)) & 0xff);
	self->value[2] = (uint8_t)((other >> (3 * 8)) & 0xff);
	self->value[3] = (uint8_t)((other >> (2 * 8)) & 0xff);
	self->value[4] = (uint8_t)((other >> (1 * 8)) & 0xff);
	self->value[5] = (uint8_t)((other >> (0 * 8)) & 0xff);
}

uint64_t eui48_convert_to_uint64(const struct eui48 *self)
{
	uint64_t v = 0;
	v |= ((uint64_t)self->value[0]) << (5 * 8);
	v |= ((uint64_t)self->value[1]) << (4 * 8);
	v |= ((uint64_t)self->value[2]) << (3 * 8);
	v |= ((uint64_t)self->value[3]) << (2 * 8);
	v |= ((uint64_t)self->value[4]) << (1 * 8);
	v |= ((uint64_t)self->value[5]) << (0 * 8);
	return v;
}

void eui48_copy(struct eui48 *self, const struct eui48 *other)
{
	int i;
	for (i = 0; i < sizeof(self->value); ++i) {
		self->value[i] = other->value[i];
	}
}

int eui48_compare(const struct eui48 *self, const struct eui48 *other)
{
	int r = memcmp(self->value, other->value, sizeof(self->value));
	return r;
}

void pdu_eui48_set(struct eui48 v, void *base, int pos)
{
	memcpy(((uint8_t *)base) + pos, v.value, sizeof(v.value));
}

int eui48_is_unset(struct eui48 v)
{
	int r = 0;
	if (v.value[0] == 0xff && v.value[1] == 0xff && v.value[2] == 0xff &&
	    v.value[3] == 0xff && v.value[4] == 0xff && v.value[5] == 0xff) {
		r = 1;
	}
	return r;
}

int eui48_is_set(struct eui48 v)
{
	int r = 0;
	if (v.value[0] != 0xff || v.value[1] != 0xff || v.value[2] != 0xff ||
	    v.value[3] != 0xff || v.value[4] != 0xff || v.value[5] != 0xff) {
		r = 1;
	}
	return r;
}

int eui48_is_zero(struct eui48 v)
{
	int r = 0;
	if (v.value[0] == 0x00 && v.value[1] == 0x00 && v.value[2] == 0x00 &&
	    v.value[3] == 0x00 && v.value[4] == 0x00 && v.value[5] == 0x00) {
		r = 1;
	}
	return r;
}

int eui48_is_not_zero(struct eui48 v)
{
	int r = 0;
	if (v.value[0] != 0x00 || v.value[1] != 0x00 || v.value[2] != 0x00 ||
	    v.value[3] != 0x00 || v.value[4] != 0x00 || v.value[5] != 0x00) {
		r = 1;
	}
	return r;
}
