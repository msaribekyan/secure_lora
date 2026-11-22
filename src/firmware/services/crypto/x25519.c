#include "x25519.h"

#define MODP_SIZE 32

typedef uint8_t modp_t[MODP_SIZE];

static const uint16_t four_p[MODP_SIZE] = {
	948,
	1020, 1020, 1020, 1020, 1020, 1020, 1020, 1020,
	1020, 1020, 1020, 1020, 1020, 1020, 1020, 1020,
	1020, 1020, 1020, 1020, 1020, 1020, 1020, 1020,
	1020, 1020, 1020, 1020, 1020, 1020,
	508,
};

static void mod_reduce(modp_t r, uint32_t carry)
{
	uint32_t acc = carry * 38;

	for (uint8_t pass = 0;pass < 2;pass++)
	{
		for (uint8_t i = 0;i < MODP_SIZE;i++)
		{
			acc += r[i];
			r[i] = (uint8_t) (acc & 0xFF);
			acc >>= 8;
		}
		acc *= 38;
	}
}

static void mod_add(modp_t r, const modp_t a, const modp_t b)
{
	uint32_t acc = 0;

	for (uint8_t i = 0;i < MODP_SIZE;i++)
	{
		acc += (uint32_t) a[i] + b[i];
		r[i] = (uint8_t) (acc & 0xFF);
		acc >>= 8;
	}
	mod_reduce(r, acc);
}

static void mod_sub(modp_t r, const modp_t a, const modp_t b)
{
	uint32_t acc = 0;

	for (uint8_t i = 0;i < MODP_SIZE;i++)
	{
		acc += (uint32_t) a[i] + four_p[i] - b[i];
		r[i] = (uint8_t) (acc & 0xFF);
		acc >>= 8;
	}
	mod_reduce(r, acc);
}

static void mod_mul(modp_t r, const modp_t a, const modp_t b)
{
	modp_t t;
	uint32_t acc = 0;

	for (uint8_t k = 0;k < MODP_SIZE;k++)
	{
		uint32_t lo = 0;
		uint32_t hi = 0;

		for (uint8_t i = 0;i <= k;i++)
			lo += (uint32_t) a[i] * b[k - i];
		for (uint8_t i = k + 1;i < MODP_SIZE;i++)
			hi += (uint32_t) a[i] * b[k + MODP_SIZE - i];
		acc += lo + 38 * hi;
		t[k] = (uint8_t) (acc & 0xFF);
		acc >>= 8;
	}

	mod_reduce(t, acc);
	for (uint8_t i = 0;i < MODP_SIZE;i++)
		r[i] = t[i];
}

static void mod_sqr(modp_t r, const modp_t a)
{
	mod_mul(r, a, a);
}

static void mod_unpack(modp_t r, const uint8_t in[MODP_SIZE])
{
	for (uint8_t i = 0;i < MODP_SIZE;i++)
		r[i] = in[i];
	r[MODP_SIZE - 1] &= 0x7F;
}

static void mod_pack(uint8_t out[MODP_SIZE], const modp_t in)
{
	modp_t t;
	modp_t u;
	uint32_t acc;
	uint8_t mask;

	acc = 19u * (uint32_t) (in[MODP_SIZE - 1] >> 7);
	for (uint8_t i = 0;i < MODP_SIZE;i++)
	{
		acc += (i == MODP_SIZE - 1) ? (in[i] & 0x7F) : in[i];
		t[i] = (uint8_t) (acc & 0xFF);
		acc >>= 8;
	}

	acc = 19;
	for (uint8_t i = 0;i < MODP_SIZE;i++)
	{
		acc += t[i];
		u[i] = (uint8_t) (acc & 0xFF);
		acc >>= 8;
	}
	mask = (uint8_t) (0u - (uint32_t) (u[MODP_SIZE - 1] >> 7));
	u[MODP_SIZE - 1] &= 0x7F;

	for (uint8_t i = 0;i < MODP_SIZE;i++)
		out[i] = (uint8_t) ((t[i] & (uint8_t) ~mask) | (u[i] & mask));
}

static void mod_sqr_n(modp_t r, const modp_t a, uint8_t n)
{
	mod_sqr(r, a);
	for (uint8_t i = 1;i < n;i++)
		mod_sqr(r, r);
}

static void mod_inv(modp_t r, const modp_t a)
{
	modp_t z2, z9, z11, z_5, z_10, z_20, z_50, z_100, t;

	mod_sqr(z2, a);
	mod_sqr_n(t, z2, 2);
	mod_mul(z9, t, a);
	mod_mul(z11, z9, z2);
	mod_sqr(t, z11);
	mod_mul(z_5, t, z9);

	mod_sqr_n(t, z_5, 5);
	mod_mul(z_10, t, z_5);
	mod_sqr_n(t, z_10, 10);
	mod_mul(z_20, t, z_10);
	mod_sqr_n(t, z_20, 20);
	mod_mul(t, t, z_20);
	mod_sqr_n(t, t, 10);
	mod_mul(z_50, t, z_10);
	mod_sqr_n(t, z_50, 50);
	mod_mul(z_100, t, z_50);
	mod_sqr_n(t, z_100, 100);
	mod_mul(t, t, z_100);
	mod_sqr_n(t, t, 50);
	mod_mul(t, t, z_50);
	mod_sqr_n(t, t, 5);
	mod_mul(r, t, z11);
}

static void mod_cswap(uint8_t swap, modp_t a, modp_t b)
{
	uint8_t mask = (uint8_t) (0u - (uint32_t) swap);

	for (uint8_t i = 0;i < MODP_SIZE;i++)
	{
		uint8_t d = (uint8_t) (mask & (a[i] ^ b[i]));

		a[i] ^= d;
		b[i] ^= d;
	}
}

static const modp_t a24 = {0x41, 0xDB, 0x01};

static void ladder(modp_t x2, modp_t z2, const uint8_t k[MODP_SIZE], const modp_t x1)
{
	modp_t x3, z3, a, b, c, d, e;
	uint8_t kc[MODP_SIZE];
	uint8_t swap = 0;
	uint8_t bit;
	volatile uint8_t *wipe = kc;

	for (uint8_t i = 0;i < MODP_SIZE;i++)
	{
		kc[i] = k[i];
		x2[i] = 0;
		z2[i] = 0;
		z3[i] = 0;
		x3[i] = x1[i];
	}
	kc[0] &= 248;
	kc[MODP_SIZE - 1] &= 127;
	kc[MODP_SIZE - 1] |= 64;
	x2[0] = 1;
	z3[0] = 1;

	for (int16_t t = 254;t >= 0;t--)
	{
		bit = (uint8_t) ((kc[t >> 3] >> (t & 7)) & 1);
		swap ^= bit;
		mod_cswap(swap, x2, x3);
		mod_cswap(swap, z2, z3);
		swap = bit;

		mod_add(a, x2, z2);
		mod_sub(b, x2, z2);
		mod_add(c, x3, z3);
		mod_sub(d, x3, z3);
		mod_mul(d, d, a);
		mod_mul(c, c, b);
		mod_sqr(a, a);
		mod_sqr(b, b);
		mod_sub(e, a, b);
		mod_add(x3, d, c);
		mod_sqr(x3, x3);
		mod_sub(z3, d, c);
		mod_sqr(z3, z3);
		mod_mul(z3, z3, x1);
		mod_mul(x2, a, b);
		mod_mul(z2, e, a24);
		mod_add(z2, z2, a);
		mod_mul(z2, z2, e);
	}
	mod_cswap(swap, x2, x3);
	mod_cswap(swap, z2, z3);

	for (uint8_t i = 0;i < MODP_SIZE;i++)
		wipe[i] = 0;
}

static void scalar_mult(modp_t out, const uint8_t k[MODP_SIZE], const modp_t u)
{
	modp_t x2;
	modp_t z2;

	ladder(x2, z2, k, u);
	mod_inv(z2, z2);
	mod_mul(out, x2, z2);
}

static const uint8_t base_point[MODP_SIZE] = {9};

int x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32])
{
	modp_t u;
	modp_t r;
	uint8_t acc = 0;
	uint32_t is_zero;
	volatile uint8_t *wipe_u = u;
	volatile uint8_t *wipe_r = r;

	mod_unpack(u, point);
	scalar_mult(r, scalar, u);
	mod_pack(out, r);

	for (uint8_t i = 0;i < MODP_SIZE;i++)
		acc |= out[i];

	is_zero = ((uint32_t) acc - 1u) >> 31;

	for (uint8_t i = 0;i < MODP_SIZE;i++)
	{
		wipe_u[i] = 0;
		wipe_r[i] = 0;
	}
	return -(int) is_zero;
}

int x25519_base_point_mult(uint8_t out[32], const uint8_t scalar[32])
{
	return x25519(out, scalar, base_point);
}

int x25519_multscalar(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32])
{
	return x25519(out, scalar, point);
}
