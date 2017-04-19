/*
 * NVMe over Fabrics Distributed Endpoint Manager (NVMe-oF DEM).
 * Copyright (c) 2017 Intel Corporation., Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define is_whitespace(c) \
	(c == ' ' || c == '\t' || c == '\r')
#define is_quote(c) (c == '\'' || c == '"')
#define is_eol(c) (c == '\n' || c == EOF)
#define COMMENT '#'
#define EQUAL '='

int consume_line(FILE *fd)
{
	char c;

	while (!feof(fd)) {
		c = fgetc(fd);
		if (is_eol(c))
			return 0;
	}
	return 1;
}

int consume_whitespace(FILE *fd)
{
	char c;

	while (!feof(fd)) {
		c = fgetc(fd);
		if (c == COMMENT) {
			return 1;
		}
		if (!is_whitespace(c) || is_eol(c)) {
			ungetc(c, fd);
			return 0;
		}
	}
	return 1;
}

int parse_word(FILE *fd, char *word, int n)
{
	char c, *p = word;
	int quoted = 0;

	while (!feof(fd)) {
		c = fgetc(fd);
		if (c == COMMENT && !quoted) {
			break;
		}
		if (is_quote(c)) {
			quoted = ~quoted;
			continue;
		}
		if (is_whitespace(c) || is_eol(c) || c == EQUAL) {
			ungetc(c, fd);
			break;
		}
		*p++ = c;
		if (--n == 0) {
			break;
		}
	}
	if (p == word)
		return 1;

	*p = 0;
	return 0;
}

int parse_equal(FILE *fd, char *word, int n)
{
	char c, *p = word;

	while (!feof(fd)) {
		c = fgetc(fd);
		if (c == COMMENT) {
			break;
		}
		if (c != EQUAL) {
			ungetc(c, fd);
			break;
		}
		*p++ = c;
		if (--n == 0) {
			break;
		}
	}
	if (p == word)
		return 1;

	*p = 0;
	return 0;
}

#define MAX_EQUAL 2

int parse_line(FILE *fd, char *tag, int tag_max, char *value, int value_max)
{
	char equal[MAX_EQUAL + 1];
	int ret;

	ret = consume_whitespace(fd);
	if (ret)
		goto out;
	ret = parse_word(fd, tag, tag_max);
	if (ret)
		goto out;
	ret = consume_whitespace(fd);
	if (ret)
		goto out;
	ret = parse_equal(fd, equal, MAX_EQUAL);
	if (ret)
		goto out;
	if (strcmp(equal, "=") != 0) {
		ret = -1;
		goto out;
	}
	ret = consume_whitespace(fd);
	if (ret)
		goto out;
	ret = parse_word(fd, value, value_max);
out:
	consume_line(fd);

	return ret;
}

static int string_to_addr(char *p, int *addr, int len, int width, char delim)
{
	char nibble[8];
	int i, j, n, z;

	n = strlen(p) + 1;
	for (z = j = i = 0; i < n && j < len; i++, p++) {
		if (*p == delim || *p == 0 || *p == '/') {
			if (z) {
				if (z > width)
					return -1;
				nibble[z] = 0;
				z = 0;
				addr[j] = atoi(nibble);
			} else
				addr[j] = 0;
			j++;
			if (*p == '/')
				break;
		} else
			nibble[z++] = *p;
	}

	for (; j < len; j++)
		addr[j] = 0;

	if (*p == '/')
		return atoi(++p);

	return 0;
}

static void print_addr(int *addr, int len, char delim, char *format)
{
	int i;
	char d[2] = { delim, 0 };

	for (i = 0; i < len; i++)
		printf(format, i ? d : "", addr[i]);
	printf("\n");
}

static void addr_mask(int *mask, int bits, int len, int width)
{
	int i, j;

	for (i = 0; i < len; i++) {
		mask[i] = 0;
		for (j = 0; j < width; j++, bits--)
			mask[i] = (mask[i] * 2) + ((bits > 0) ? 1 : 0);
	}
}

static int addr_equal(int *addr, int *dest, int *mask, int len)
{
	int i;

	for (i = 0; i < len; i++)
		if ((addr[i] & mask[i]) != (dest[i] & mask[i]))
			return 0;

	return 1;
}

#define IPV4_LEN	4
#define IPV4_BITS	8
#define IPV4_WIDTH	3
#define IPV4_DELIM	'.'
#define IPV4_FORMAT	"%s%d"

#define IPV6_LEN	8
#define IPV6_BITS	16
#define IPV6_WIDTH	4
#define IPV6_DELIM	':'
#define IPV6_FORMAT	"%s%x"

#define FC_LEN		8
#define FC_BITS		8
#define FC_WIDTH	2
#define FC_DELIM	':'
#define FC_FORMAT	"%s%02x"

int ipv4_to_addr(char *p, int *addr)
{
	return string_to_addr(p, addr, IPV4_LEN, IPV4_WIDTH, IPV4_DELIM);
}
void print_ipv4(int *addr)
{
	print_addr(addr, IPV4_LEN, IPV4_DELIM, IPV4_FORMAT);
}
void ipv4_mask(int *mask, int bits)
{
	addr_mask(mask, bits, IPV4_LEN, IPV4_BITS);
}
int ipv4_equal(int *addr, int *dest, int *mask)
{
	return addr_equal(addr, dest, mask, IPV4_LEN);
}

int ipv6_to_addr(char *p, int *addr)
{
	return string_to_addr(p, addr, IPV6_LEN, IPV6_WIDTH, IPV6_DELIM);
}
void print_ipv6(int *addr)
{
	print_addr(addr, IPV6_LEN, IPV6_DELIM, IPV6_FORMAT);
}
void ipv6_mask(int *mask, int bits)
{
	addr_mask(mask, bits, IPV6_LEN, IPV6_BITS);
}
int ipv6_equal(int *addr, int *dest, int *mask)
{
	return addr_equal(addr, dest, mask, IPV6_LEN);
}

int fc_to_addr(char *p, int *addr)
{
	return string_to_addr(p, addr, FC_LEN, FC_WIDTH, FC_DELIM);
}
void print_fc(int *addr)
{
	print_addr(addr, FC_LEN, FC_DELIM, FC_FORMAT);
}
void fc_mask(int *mask, int bits)
{
	addr_mask(mask, bits, FC_LEN, FC_BITS);
}
int fc_equal(int *addr, int *dest, int *mask)
{
	return addr_equal(addr, dest, mask, FC_LEN);
}
