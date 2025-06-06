/*
 * Copyright (c) 2001-2004 Jakub Jermar
 * Copyright (c) 2005 Martin Decky
 * Copyright (c) 2008 Jiri Svoboda
 * Copyright (c) 2011 Martin Sucha
 * Copyright (c) 2011 Oleg Romanenko
 * Copyright (c) 2025 Jiří Zárevúcky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup libc
 * @{
 */

/**
 * @file
 * @brief String functions.
 *
 * Strings and characters use the Universal Character Set (UCS). The standard
 * strings, called just strings are encoded in UTF-8. Wide strings (encoded
 * in UTF-32) are supported to a limited degree. A single character is
 * represented as char32_t.@n
 *
 * Overview of the terminology:@n
 *
 *  Term                  Meaning
 *  --------------------  ----------------------------------------------------
 *  byte                  8 bits stored in uint8_t (unsigned 8 bit integer)
 *
 *  character             UTF-32 encoded Unicode character, stored in char32_t
 *                        (unsigned 32 bit integer), code points 0 .. 1114111
 *                        are valid
 *
 *                        Note that Unicode characters do not match
 *                        one-to-one with displayed characters or glyphs on
 *                        screen. For that level of precision, look up
 *                        Grapheme Clusters.
 *
 *  ASCII character       7 bit encoded ASCII character, stored in char
 *                        (usually signed 8 bit integer), code points 0 .. 127
 *                        are valid
 *
 *  string                UTF-8 encoded NULL-terminated Unicode string, char *
 *
 *  wide string           UTF-32 encoded NULL-terminated Unicode string,
 *                        char32_t *
 *
 *  [wide] string size    number of BYTES in a [wide] string (excluding
 *                        the NULL-terminator), size_t
 *
 *  [wide] string length  number of CHARACTERS in a [wide] string (excluding
 *                        the NULL-terminator), size_t
 *
 *  [wide] string width   number of display cells on a monospace display taken
 *                        by a [wide] string, size_t
 *
 *                        This is virtually impossible to determine exactly for
 *                        all strings without knowing specifics of the display
 *                        device, due to various factors affecting text output.
 *                        If you have the option to query the terminal for
 *                        position change caused by outputting the string,
 *                        it is preferrable to determine width that way.
 *
 *
 * Overview of string metrics:@n
 *
 *  Metric  Abbrev.  Type     Meaning
 *  ------  ------   ------   -------------------------------------------------
 *  size    n        size_t   number of BYTES in a string (excluding the
 *                            NULL-terminator)
 *
 *  length  l        size_t   number of CHARACTERS in a string (excluding the
 *                            null terminator)
 *
 *  width  w         size_t   number of display cells on a monospace display
 *                            taken by a string
 *
 *
 * Function naming prefixes:@n
 *
 *  chr_    operate on characters
 *  ascii_  operate on ASCII characters
 *  str_    operate on strings
 *  wstr_   operate on wide strings
 *
 *  [w]str_[n|l|w]  operate on a prefix limited by size, length
 *                  or width
 *
 *
 * A specific character inside a [wide] string can be referred to by:@n
 *
 *  pointer (char *, char32_t *)
 *  byte offset (size_t)
 *  character index (size_t)
 *
 */

#include <str.h>

#include <align.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <macros.h>
#include <mem.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <uchar.h>

#if __STDC_HOSTED__
#include <fibril.h>
#endif

static void _set_ilseq()
{
#ifdef errno
	errno = EILSEQ;
#endif
}

/** Byte mask consisting of lowest @n bits (out of 8) */
#define LO_MASK_8(n)  ((uint8_t) ((1 << (n)) - 1))

/** Byte mask consisting of lowest @n bits (out of 32) */
#define LO_MASK_32(n)  ((uint32_t) ((1 << (n)) - 1))

/** Byte mask consisting of highest @n bits (out of 8) */
#define HI_MASK_8(n)  (~LO_MASK_8(8 - (n)))

/** Number of data bits in a UTF-8 continuation byte */
#define CONT_BITS  6

#define UTF8_MASK_INITIAL2  0b00011111
#define UTF8_MASK_INITIAL3  0b00001111
#define UTF8_MASK_INITIAL4  0b00000111
#define UTF8_MASK_CONT      0b00111111

#define CHAR_INVALID ((char32_t) UINT_MAX)

static inline bool _is_ascii(uint8_t b)
{
	return b < 0x80;
}

static inline bool _is_continuation(uint8_t b)
{
	return (b & 0xC0) == 0x80;
}

static inline bool _is_2_byte(uint8_t c)
{
	return (c & 0xE0) == 0xC0;
}

static inline bool _is_3_byte(uint8_t c)
{
	return (c & 0xF0) == 0xE0;
}

static inline bool _is_4_byte(uint8_t c)
{
	return (c & 0xF8) == 0xF0;
}

static inline int _char_continuation_bytes(char32_t c)
{
	if ((c & ~LO_MASK_32(7)) == 0)
		return 0;

	if ((c & ~LO_MASK_32(11)) == 0)
		return 1;

	if ((c & ~LO_MASK_32(16)) == 0)
		return 2;

	if ((c & ~LO_MASK_32(21)) == 0)
		return 3;

	/* Codes longer than 21 bits are not supported */
	return -1;
}

static inline int _continuation_bytes(uint8_t b)
{
	/* 0xxxxxxx */
	if (_is_ascii(b))
		return 0;

	/* 110xxxxx 10xxxxxx */
	if (_is_2_byte(b))
		return 1;

	/* 1110xxxx 10xxxxxx 10xxxxxx */
	if (_is_3_byte(b))
		return 2;

	/* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
	if (_is_4_byte(b))
		return 3;

	return -1;
}

static bool _is_non_shortest(const mbstate_t *mb, uint8_t b)
{
	return (mb->state == 0b1111110000000000 && !(b & 0b00100000)) ||
	    (mb->state == 0b1111111111110000 && !(b & 0b00110000));
}

static bool _is_surrogate(const mbstate_t *mb, uint8_t b)
{
	return (mb->state == 0b1111110000001101 && b >= 0xa0);
}

#define _likely(expr) __builtin_expect((expr), true)
#define _unlikely(expr) __builtin_expect((expr), false)

#define FAST_PATHS 1

static char32_t _str_decode(const char *s, size_t *offset, size_t size, mbstate_t *mb)
{
	assert(s);
	assert(offset);
	assert(*offset <= size);
	assert(size == STR_NO_LIMIT || s + size >= s);
	assert(mb);

	if (*offset == size)
		return 0;

	if (_likely(!mb->state)) {
		/* Clean slate, read initial byte. */
		uint8_t b = s[(*offset)++];

		/* Fast exit for the most common case. */
		if (_likely(_is_ascii(b)))
			return b;

		/* unexpected continuation byte */
		if (_unlikely(_is_continuation(b)))
			return CHAR_INVALID;

		/*
		 * The value stored into `continuation` is designed to have
		 * just enough leading ones that after shifting in one less than
		 * the expected number of continuation bytes, the most significant
		 * bit becomes zero. (The field is 16b wide.)
		 */

		if (_is_2_byte(b)) {
			/* Reject non-shortest form. */
			if (_unlikely(!(b & 0b00011110)))
				return CHAR_INVALID;

#if FAST_PATHS
			/* We can usually take this exit. */
			if (_likely(*offset < size && _is_continuation(s[*offset])))
				return (b & UTF8_MASK_INITIAL2) << 6 |
				    (s[(*offset)++] & UTF8_MASK_CONT);
#endif

			/* 2 byte continuation    110xxxxx */
			mb->state = b ^ 0b0000000011000000;

		} else if (_is_3_byte(b)) {
#if FAST_PATHS
			/* We can usually take this exit. */
			if (_likely(*offset + 1 < size && _is_continuation(s[*offset]) && _is_continuation(s[*offset + 1]))) {

				char32_t ch = (b & UTF8_MASK_INITIAL3) << 12 |
				    (s[(*offset)] & UTF8_MASK_CONT) << 6 |
				    (s[(*offset) + 1] & UTF8_MASK_CONT);

				*offset += 2;

				/* Reject non-shortest form. */
				if (_unlikely(!(ch & 0xFFFFF800)))
					return CHAR_INVALID;

				/* Reject surrogates */
				if (_unlikely(ch >= 0xD800 && ch < 0xE000))
					return CHAR_INVALID;

				return ch;
			}
#endif

			/* 3 byte continuation    1110xxxx */
			mb->state = b ^ 0b1111110011100000;

		} else if (_is_4_byte(b)) {
#if FAST_PATHS
			/* We can usually take this exit. */
			if (_likely(*offset + 2 < size && _is_continuation(s[*offset]) &&
			    _is_continuation(s[*offset + 1]) && _is_continuation(s[*offset + 2]))) {

				char32_t ch = (b & UTF8_MASK_INITIAL4) << 18 |
				    (s[(*offset)] & UTF8_MASK_CONT) << 12 |
				    (s[(*offset) + 1] & UTF8_MASK_CONT) << 6 |
				    (s[(*offset) + 2] & UTF8_MASK_CONT);

				*offset += 3;

				/* Reject non-shortest form. */
				if (_unlikely(!(ch & 0xFFFF0000)))
					return CHAR_INVALID;

				/* Reject out-of-range characters. */
				if (_unlikely(ch >= 0x110000))
					return CHAR_INVALID;

				return ch;
			}
#endif

			/* 4 byte continuation    11110xxx */
			mb->state = b ^ 0b1111111100000000;
		} else {
			return CHAR_INVALID;
		}
	}

	/* Deal with the remaining edge and invalid cases. */
	for (; *offset < size; (*offset)++) {
		/* Read continuation bytes. */
		uint8_t b = s[*offset];

		if (!_is_continuation(b) || _is_non_shortest(mb, b) || _is_surrogate(mb, b)) {
			mb->state = 0;
			return CHAR_INVALID;
		}

		/* Top bit becomes zero when shifting in the second to last byte. */
		if (!(mb->state & 0x8000)) {
			char32_t c = ((char32_t) mb->state) << 6 | (b & UTF8_MASK_CONT);
			mb->state = 0;
			(*offset)++;
			return c;
		}

		mb->state = mb->state << 6 | (b & UTF8_MASK_CONT);
	}

	/* Incomplete character. */
	assert(mb->state);
	return 0;
}

/** Standard <uchar.h> function since C11. */
size_t mbrtoc32(char32_t *c, const char *s, size_t n, mbstate_t *mb)
{
#if __STDC_HOSTED__
	static fibril_local mbstate_t global_state = { };

	if (!mb)
		mb = &global_state;
#endif

	if (!s) {
		/* Equivalent to mbrtoc32(NULL, "", 1, mb); */
		c = NULL;
		s = "";
		n = 1;
	}

	size_t offset = 0;
	char32_t ret = _str_decode(s, &offset, n, mb);
	if (ret == CHAR_INVALID) {
		assert(!mb->state);
		_set_ilseq();
		return UCHAR_ILSEQ;
	}
	if (mb->state) {
		assert(ret == 0);
		return UCHAR_INCOMPLETE;
	}

	if (c)
		*c = ret;
	return ret ? offset : 0;
}

/** Decode a single character from a string.
 *
 * Decode a single character from a string of size @a size. Decoding starts
 * at @a offset and this offset is moved to the beginning of the next
 * character. In case of decoding error, offset generally advances at least
 * by one. However, offset is never moved beyond size.
 *
 * @param str    String (not necessarily NULL-terminated).
 * @param offset Byte offset in string where to start decoding.
 * @param size   Size of the string (in bytes).
 *
 * @return Value of decoded character, U_SPECIAL on decoding error or
 *         NULL if attempt to decode beyond @a size.
 *
 */
char32_t str_decode(const char *str, size_t *offset, size_t size)
{
	mbstate_t mb = { };
	char32_t ch = _str_decode(str, offset, size, &mb);

	if (ch == CHAR_INVALID || mb.state)
		return U_SPECIAL;

	return ch;
}

char32_t str_decode_r(const char *str, size_t *offset, size_t size,
	char32_t replacement, mbstate_t *mb)
{
	char32_t ch = _str_decode(str, offset, size, mb);
	return (ch == CHAR_INVALID) ? replacement : ch;
}

/** Decode a single character from a string to the left.
 *
 * Decode a single character from a string of size @a size. Decoding starts
 * at @a offset and this offset is moved to the beginning of the previous
 * character. In case of decoding error, offset generally decreases at least
 * by one. However, offset is never moved before 0.
 *
 * @param str    String (not necessarily NULL-terminated).
 * @param offset Byte offset in string where to start decoding.
 * @param size   Size of the string (in bytes).
 *
 * @return Value of decoded character, U_SPECIAL on decoding error or
 *         NULL if attempt to decode beyond @a start of str.
 *
 */
char32_t str_decode_reverse(const char *str, size_t *offset, size_t size)
{
	if (*offset == 0)
		return 0;

	int cbytes = 0;
	/* Continue while continuation bytes found */
	while (*offset > 0 && cbytes < 4) {
		uint8_t b = (uint8_t) str[--(*offset)];

		if (_is_continuation(b)) {
			cbytes++;
			continue;
		}

		/* Reject non-shortest form encoding. */
		if (cbytes != _continuation_bytes(b))
			return U_SPECIAL;

		/* Start byte */
		size_t start_offset = *offset;
		return str_decode(str, &start_offset, size);
	}

	/* Too many continuation bytes */
	return U_SPECIAL;
}

/** Encode a single character to string representation.
 *
 * Encode a single character to string representation (i.e. UTF-8) and store
 * it into a buffer at @a offset. Encoding starts at @a offset and this offset
 * is moved to the position where the next character can be written to.
 *
 * @param ch     Input character.
 * @param str    Output buffer.
 * @param offset Byte offset where to start writing.
 * @param size   Size of the output buffer (in bytes).
 *
 * @return EOK if the character was encoded successfully, EOVERFLOW if there
 *         was not enough space in the output buffer or EINVAL if the character
 *         code was invalid.
 */
errno_t chr_encode(char32_t ch, char *str, size_t *offset, size_t size)
{
	// TODO: merge with c32rtomb()

	if (*offset >= size)
		return EOVERFLOW;

	/* Fast exit for the most common case. */
	if (ch < 0x80) {
		str[(*offset)++] = (char) ch;
		return EOK;
	}

	/* Codes longer than 21 bits are not supported */
	if (!chr_check(ch))
		return EINVAL;

	/* Determine how many continuation bytes are needed */

	unsigned int cbytes = _char_continuation_bytes(ch);
	unsigned int b0_bits = 6 - cbytes;  /* Data bits in first byte */

	/* Check for available space in buffer */
	if (*offset + cbytes >= size)
		return EOVERFLOW;

	/* Encode continuation bytes */
	unsigned int i;
	for (i = cbytes; i > 0; i--) {
		str[*offset + i] = 0x80 | (ch & LO_MASK_32(CONT_BITS));
		ch >>= CONT_BITS;
	}

	/* Encode first byte */
	str[*offset] = (ch & LO_MASK_32(b0_bits)) | HI_MASK_8(8 - b0_bits - 1);

	/* Advance offset */
	*offset += cbytes + 1;

	return EOK;
}

/* Convert in place any bytes that don't form a valid character into replacement. */
static size_t _str_sanitize(char *str, size_t n, uint8_t replacement)
{
	uint8_t *b = (uint8_t *) str;
	size_t count = 0;

	for (; n > 0 && b[0]; b++, n--) {
		if (b[0] < ' ') {
			/* C0 control codes */
			b[0] = replacement;
			count++;
			continue;
		}

		int cont = _continuation_bytes(b[0]);
		if (__builtin_expect(cont, 0) == 0)
			continue;

		if (cont < 0 || n <= (size_t) cont) {
			b[0] = replacement;
			count++;
			continue;
		}

		/* Check continuation bytes. */
		bool valid = true;
		for (int i = 1; i <= cont; i++) {
			if (!_is_continuation(b[i])) {
				valid = false;
				break;
			}
		}

		if (!valid) {
			b[0] = replacement;
			count++;
			continue;
		}

		/*
		 * Check for non-shortest form encoding.
		 * See https://www.unicode.org/versions/corrigendum1.html
		 */

		/* 0b110!!!!x 0b10xxxxxx */
		if (cont == 1 && !(b[0] & 0b00011110)) {
			b[0] = replacement;
			count++;
			continue;
		}

		bool c1_control = (b[0] == 0b11000010 && b[1] < 0b10100000);
		if (cont == 1 && c1_control) {
			b[0] = replacement;
			count++;
			continue;
		}

		/* 0b1110!!!! 0b10!xxxxx 0b10xxxxxx */
		if (cont == 2 && !(b[0] & 0b00001111) && !(b[1] & 0b00100000)) {
			b[0] = replacement;
			count++;
			continue;
		}

		/* 0b11110!!! 0b10!!xxxx 0b10xxxxxx 0b10xxxxxx */
		if (cont == 3 && !(b[0] & 0b00000111) && !(b[1] & 0b00110000)) {
			b[0] = replacement;
			count++;
			continue;
		}

		/* Check for surrogate character encoding. */
		if (cont == 2 && b[0] == 0xED && b[1] >= 0xA0) {
			b[0] = replacement;
			count++;
			continue;
		}

		/* Check for out-of-range code points. */
		if (cont == 3 && (b[0] > 0xF4 || (b[0] == 0xF4 && b[1] >= 0x90))) {
			b[0] = replacement;
			count++;
			continue;
		}

		b += cont;
		n -= cont;
	}

	return count;
}

/** Replaces any byte that's not part of a complete valid UTF-8 character
 * encoding with a replacement byte.
 * Also replaces C0 and C1 control codes.
 */
size_t str_sanitize(char *str, size_t n, uint8_t replacement)
{
	return _str_sanitize(str, n, replacement);
}

static size_t _str_size(const char *str)
{
	size_t size = 0;

	while (*str++ != 0)
		size++;

	return size;
}

/** Get size of string.
 *
 * Get the number of bytes which are used by the string @a str (excluding the
 * NULL-terminator).
 *
 * @param str String to consider.
 *
 * @return Number of bytes used by the string
 *
 */
size_t str_size(const char *str)
{
	return _str_size(str);
}

/** Get size of wide string.
 *
 * Get the number of bytes which are used by the wide string @a str (excluding the
 * NULL-terminator).
 *
 * @param str Wide string to consider.
 *
 * @return Number of bytes used by the wide string
 *
 */
size_t wstr_size(const char32_t *str)
{
	return (wstr_length(str) * sizeof(char32_t));
}

/** Get size of string with length limit.
 *
 * Get the number of bytes which are used by up to @a max_len first
 * characters in the string @a str. If @a max_len is greater than
 * the length of @a str, the entire string is measured (excluding the
 * NULL-terminator).
 *
 * @param str     String to consider.
 * @param max_len Maximum number of characters to measure.
 *
 * @return Number of bytes used by the characters.
 *
 */
size_t str_lsize(const char *str, size_t max_len)
{
	size_t len = 0;
	size_t offset = 0;

	while (len < max_len) {
		if (str_decode(str, &offset, STR_NO_LIMIT) == 0)
			break;

		len++;
	}

	return offset;
}

static size_t _str_nsize(const char *str, size_t max_size)
{
	size_t size = 0;

	while ((*str++ != 0) && (size < max_size))
		size++;

	return size;
}

/** Get size of string with size limit.
 *
 * Get the number of bytes which are used by the string @a str
 * (excluding the NULL-terminator), but no more than @max_size bytes.
 *
 * @param str      String to consider.
 * @param max_size Maximum number of bytes to measure.
 *
 * @return Number of bytes used by the string
 *
 */
size_t str_nsize(const char *str, size_t max_size)
{
	return _str_nsize(str, max_size);
}

/** Get size of wide string with size limit.
 *
 * Get the number of bytes which are used by the wide string @a str
 * (excluding the NULL-terminator), but no more than @max_size bytes.
 *
 * @param str      Wide string to consider.
 * @param max_size Maximum number of bytes to measure.
 *
 * @return Number of bytes used by the wide string
 *
 */
size_t wstr_nsize(const char32_t *str, size_t max_size)
{
	return (wstr_nlength(str, max_size) * sizeof(char32_t));
}

/** Get size of wide string with length limit.
 *
 * Get the number of bytes which are used by up to @a max_len first
 * wide characters in the wide string @a str. If @a max_len is greater than
 * the length of @a str, the entire wide string is measured (excluding the
 * NULL-terminator).
 *
 * @param str     Wide string to consider.
 * @param max_len Maximum number of wide characters to measure.
 *
 * @return Number of bytes used by the wide characters.
 *
 */
size_t wstr_lsize(const char32_t *str, size_t max_len)
{
	return (wstr_nlength(str, max_len * sizeof(char32_t)) * sizeof(char32_t));
}

/** Get number of characters in a string.
 *
 * @param str NULL-terminated string.
 *
 * @return Number of characters in string.
 *
 */
size_t str_length(const char *str)
{
	size_t len = 0;
	size_t offset = 0;

	while (str_decode(str, &offset, STR_NO_LIMIT) != 0)
		len++;

	return len;
}

/** Get number of characters in a wide string.
 *
 * @param str NULL-terminated wide string.
 *
 * @return Number of characters in @a str.
 *
 */
size_t wstr_length(const char32_t *wstr)
{
	size_t len = 0;

	while (*wstr++ != 0)
		len++;

	return len;
}

/** Get number of characters in a string with size limit.
 *
 * @param str  NULL-terminated string.
 * @param size Maximum number of bytes to consider.
 *
 * @return Number of characters in string.
 *
 */
size_t str_nlength(const char *str, size_t size)
{
	size_t len = 0;
	size_t offset = 0;

	while (str_decode(str, &offset, size) != 0)
		len++;

	return len;
}

/** Get number of characters in a string with size limit.
 *
 * @param str  NULL-terminated string.
 * @param size Maximum number of bytes to consider.
 *
 * @return Number of characters in string.
 *
 */
size_t wstr_nlength(const char32_t *str, size_t size)
{
	size_t len = 0;
	size_t limit = ALIGN_DOWN(size, sizeof(char32_t));
	size_t offset = 0;

	while ((offset < limit) && (*str++ != 0)) {
		len++;
		offset += sizeof(char32_t);
	}

	return len;
}

/** Get character display width on a character cell display.
 *
 * @param ch	Character
 * @return	Width of character in cells.
 */
size_t chr_width(char32_t ch)
{
	return 1;
}

/** Get string display width on a character cell display.
 *
 * @param str	String
 * @return	Width of string in cells.
 */
size_t str_width(const char *str)
{
	size_t width = 0;
	size_t offset = 0;
	char32_t ch;

	while ((ch = str_decode(str, &offset, STR_NO_LIMIT)) != 0)
		width += chr_width(ch);

	return width;
}

/** Check whether character is plain ASCII.
 *
 * @return True if character is plain ASCII.
 *
 */
bool ascii_check(char32_t ch)
{
	if (ch <= 127)
		return true;

	return false;
}

/** Check whether character is valid
 *
 * @return True if character is a valid Unicode code point.
 *
 */
bool chr_check(char32_t ch)
{
	if (ch <= 1114111)
		return true;

	return false;
}

/** Compare two NULL terminated strings.
 *
 * Do a char-by-char comparison of two NULL-terminated strings.
 * The strings are considered equal iff their length is equal
 * and both strings consist of the same sequence of characters.
 *
 * A string S1 is less than another string S2 if it has a character with
 * lower value at the first character position where the strings differ.
 * If the strings differ in length, the shorter one is treated as if
 * padded by characters with a value of zero.
 *
 * @param s1 First string to compare.
 * @param s2 Second string to compare.
 *
 * @return 0 if the strings are equal, -1 if the first is less than the second,
 *         1 if the second is less than the first.
 *
 */
int str_cmp(const char *s1, const char *s2)
{
	/*
	 * UTF-8 has the nice property that lexicographic ordering on bytes is
	 * the same as the lexicographic ordering of the character sequences.
	 */
	while (*s1 == *s2 && *s1 != 0) {
		s1++;
		s2++;
	}

	if (*s1 == *s2)
		return 0;

	return (*s1 < *s2) ? -1 : 1;
}

/** Compare two NULL terminated strings with length limit.
 *
 * Do a char-by-char comparison of two NULL-terminated strings.
 * The strings are considered equal iff
 * min(str_length(s1), max_len) == min(str_length(s2), max_len)
 * and both strings consist of the same sequence of characters,
 * up to max_len characters.
 *
 * A string S1 is less than another string S2 if it has a character with
 * lower value at the first character position where the strings differ.
 * If the strings differ in length, the shorter one is treated as if
 * padded by characters with a value of zero. Only the first max_len
 * characters are considered.
 *
 * @param s1      First string to compare.
 * @param s2      Second string to compare.
 * @param max_len Maximum number of characters to consider.
 *
 * @return 0 if the strings are equal, -1 if the first is less than the second,
 *         1 if the second is less than the first.
 *
 */
int str_lcmp(const char *s1, const char *s2, size_t max_len)
{
	char32_t c1 = 0;
	char32_t c2 = 0;

	size_t off1 = 0;
	size_t off2 = 0;

	size_t len = 0;

	while (true) {
		if (len >= max_len)
			break;

		c1 = str_decode(s1, &off1, STR_NO_LIMIT);
		c2 = str_decode(s2, &off2, STR_NO_LIMIT);

		if (c1 < c2)
			return -1;

		if (c1 > c2)
			return 1;

		if (c1 == 0 || c2 == 0)
			break;

		++len;
	}

	return 0;

}

/** Compare two NULL terminated strings in case-insensitive manner.
 *
 * Do a char-by-char comparison of two NULL-terminated strings.
 * The strings are considered equal iff their length is equal
 * and both strings consist of the same sequence of characters
 * when converted to lower case.
 *
 * A string S1 is less than another string S2 if it has a character with
 * lower value at the first character position where the strings differ.
 * If the strings differ in length, the shorter one is treated as if
 * padded by characters with a value of zero.
 *
 * @param s1 First string to compare.
 * @param s2 Second string to compare.
 *
 * @return 0 if the strings are equal, -1 if the first is less than the second,
 *         1 if the second is less than the first.
 *
 */
int str_casecmp(const char *s1, const char *s2)
{
	// FIXME: doesn't work for non-ASCII caseful characters

	char32_t c1 = 0;
	char32_t c2 = 0;

	size_t off1 = 0;
	size_t off2 = 0;

	while (true) {
		c1 = tolower(str_decode(s1, &off1, STR_NO_LIMIT));
		c2 = tolower(str_decode(s2, &off2, STR_NO_LIMIT));

		if (c1 < c2)
			return -1;

		if (c1 > c2)
			return 1;

		if (c1 == 0 || c2 == 0)
			break;
	}

	return 0;
}

/** Compare two NULL terminated strings with length limit in case-insensitive
 * manner.
 *
 * Do a char-by-char comparison of two NULL-terminated strings.
 * The strings are considered equal iff
 * min(str_length(s1), max_len) == min(str_length(s2), max_len)
 * and both strings consist of the same sequence of characters,
 * up to max_len characters.
 *
 * A string S1 is less than another string S2 if it has a character with
 * lower value at the first character position where the strings differ.
 * If the strings differ in length, the shorter one is treated as if
 * padded by characters with a value of zero. Only the first max_len
 * characters are considered.
 *
 * @param s1      First string to compare.
 * @param s2      Second string to compare.
 * @param max_len Maximum number of characters to consider.
 *
 * @return 0 if the strings are equal, -1 if the first is less than the second,
 *         1 if the second is less than the first.
 *
 */
int str_lcasecmp(const char *s1, const char *s2, size_t max_len)
{
	// FIXME: doesn't work for non-ASCII caseful characters

	char32_t c1 = 0;
	char32_t c2 = 0;

	size_t off1 = 0;
	size_t off2 = 0;

	size_t len = 0;

	while (true) {
		if (len >= max_len)
			break;

		c1 = tolower(str_decode(s1, &off1, STR_NO_LIMIT));
		c2 = tolower(str_decode(s2, &off2, STR_NO_LIMIT));

		if (c1 < c2)
			return -1;

		if (c1 > c2)
			return 1;

		if (c1 == 0 || c2 == 0)
			break;

		++len;
	}

	return 0;

}

static bool _test_prefix(const char *s, const char *p)
{
	while (*s == *p && *s != 0) {
		s++;
		p++;
	}

	return *p == 0;
}

/** Test whether p is a prefix of s.
 *
 * Do a char-by-char comparison of two NULL-terminated strings
 * and determine if p is a prefix of s.
 *
 * @param s The string in which to look
 * @param p The string to check if it is a prefix of s
 *
 * @return true iff p is prefix of s else false
 *
 */
bool str_test_prefix(const char *s, const char *p)
{
	return _test_prefix(s, p);
}

/** Get a string suffix.
 *
 * Return a string suffix defined by the prefix length.
 *
 * @param s             The string to get the suffix from.
 * @param prefix_length Number of prefix characters to ignore.
 *
 * @return String suffix.
 *
 */
const char *str_suffix(const char *s, size_t prefix_length)
{
	size_t off = 0;
	size_t i = 0;

	while (true) {
		str_decode(s, &off, STR_NO_LIMIT);
		i++;

		if (i >= prefix_length)
			break;
	}

	return s + off;
}

/** Copy string as a sequence of bytes. */
static void _str_cpy(char *dest, const char *src)
{
	while (*src)
		*(dest++) = *(src++);

	*dest = 0;
}

/** Copy string as a sequence of bytes. */
static void _str_cpyn(char *dest, size_t size, const char *src)
{
	assert(dest && src && size);

	if (!dest || !src || !size)
		return;

	if (size == STR_NO_LIMIT)
		return _str_cpy(dest, src);

	char *dest_top = dest + size - 1;
	assert(size == 1 || dest < dest_top);

	while (*src && dest < dest_top)
		*(dest++) = *(src++);

	*dest = 0;
}

/** Copy string.
 *
 * Copy source string @a src to destination buffer @a dest.
 * No more than @a size bytes are written. If the size of the output buffer
 * is at least one byte, the output string will always be well-formed, i.e.
 * null-terminated and containing only complete characters.
 *
 * @param dest  Destination buffer.
 * @param count Size of the destination buffer (must be > 0).
 * @param src   Source string.
 *
 */
void str_cpy(char *dest, size_t size, const char *src)
{
	/* There must be space for a null terminator in the buffer. */
	assert(size > 0);
	assert(src != NULL);
	assert(dest != NULL);
	assert(size == STR_NO_LIMIT || dest + size > dest);

	/* Copy data. */
	_str_cpyn(dest, size, src);

	/* In-place translate invalid bytes to U_SPECIAL. */
	_str_sanitize(dest, size, U_SPECIAL);
}

/** Copy size-limited substring.
 *
 * Copy prefix of string @a src of max. size @a size to destination buffer
 * @a dest. No more than @a size bytes are written. The output string will
 * always be well-formed, i.e. null-terminated and containing only complete
 * characters.
 *
 * No more than @a n bytes are read from the input string, so it does not
 * have to be null-terminated.
 *
 * @param dest  Destination buffer.
 * @param count Size of the destination buffer (must be > 0).
 * @param src   Source string.
 * @param n     Maximum number of bytes to read from @a src.
 *
 */
void str_ncpy(char *dest, size_t size, const char *src, size_t n)
{
	/* There must be space for a null terminator in the buffer. */
	assert(size > 0);
	assert(src != NULL);

	/* Copy data. */
	_str_cpyn(dest, min(size, n + 1), src);

	/* In-place translate invalid bytes to U_SPECIAL. */
	_str_sanitize(dest, size, U_SPECIAL);
}

/** Append one string to another.
 *
 * Append source string @a src to string in destination buffer @a dest.
 * Size of the destination buffer is @a dest. If the size of the output buffer
 * is at least one byte, the output string will always be well-formed, i.e.
 * null-terminated and containing only complete characters.
 *
 * @param dest   Destination buffer.
 * @param count Size of the destination buffer.
 * @param src   Source string.
 */
void str_append(char *dest, size_t size, const char *src)
{
	assert(src != NULL);
	assert(dest != NULL);
	assert(size > 0);
	assert(size == STR_NO_LIMIT || dest + size > dest);

	size_t dstr_size = _str_nsize(dest, size);
	if (dstr_size < size) {
		_str_cpyn(dest + dstr_size, size - dstr_size, src);
		_str_sanitize(dest + dstr_size, size - dstr_size, U_SPECIAL);
	}
}

/** Convert space-padded ASCII to string.
 *
 * Common legacy text encoding in hardware is 7-bit ASCII fitted into
 * a fixed-width byte buffer (bit 7 always zero), right-padded with spaces
 * (ASCII 0x20). Convert space-padded ascii to string representation.
 *
 * If the text does not fit into the destination buffer, the function converts
 * as many characters as possible and returns EOVERFLOW.
 *
 * If the text contains non-ASCII bytes (with bit 7 set), the whole string is
 * converted anyway and invalid characters are replaced with question marks
 * (U_SPECIAL) and the function returns EIO.
 *
 * Regardless of return value upon return @a dest will always be well-formed.
 *
 * @param dest		Destination buffer
 * @param size		Size of destination buffer
 * @param src		Space-padded ASCII.
 * @param n		Size of the source buffer in bytes.
 *
 * @return		EOK on success, EOVERFLOW if the text does not fit
 *			destination buffer, EIO if the text contains
 *			non-ASCII bytes.
 */
errno_t spascii_to_str(char *dest, size_t size, const uint8_t *src, size_t n)
{
	size_t len = 0;

	/* Determine the length of the source string. */
	for (size_t i = 0; i < n; i++) {
		if (src[i] == 0)
			break;

		if (src[i] != ' ')
			len = i + 1;
	}

	errno_t result = EOK;
	size_t out_len = min(len, size - 1);

	/* Copy characters */
	for (size_t i = 0; i < out_len; i++) {
		dest[i] = src[i];

		if (dest[i] < 0) {
			dest[i] = U_SPECIAL;
			result = EIO;
		}
	}

	dest[out_len] = 0;

	if (out_len < len)
		return EOVERFLOW;

	return result;
}

/** Convert wide string to string.
 *
 * Convert wide string @a src to string. The output is written to the buffer
 * specified by @a dest and @a size. @a size must be non-zero and the string
 * written will always be well-formed.
 *
 * @param dest	Destination buffer.
 * @param size	Size of the destination buffer.
 * @param src	Source wide string.
 */
void wstr_to_str(char *dest, size_t size, const char32_t *src)
{
	char32_t ch;
	size_t src_idx;
	size_t dest_off;

	/* There must be space for a null terminator in the buffer. */
	assert(size > 0);

	src_idx = 0;
	dest_off = 0;

	while ((ch = src[src_idx++]) != 0) {
		if (chr_encode(ch, dest, &dest_off, size - 1) != EOK)
			break;
	}

	dest[dest_off] = '\0';
}

/** Convert UTF16 string to string.
 *
 * Convert utf16 string @a src to string. The output is written to the buffer
 * specified by @a dest and @a size. @a size must be non-zero and the string
 * written will always be well-formed. Surrogate pairs also supported.
 *
 * @param dest	Destination buffer.
 * @param size	Size of the destination buffer.
 * @param src	Source utf16 string.
 *
 * @return EOK, if success, an error code otherwise.
 */
errno_t utf16_to_str(char *dest, size_t size, const uint16_t *src)
{
	size_t idx = 0, dest_off = 0;
	char32_t ch;
	errno_t rc = EOK;

	/* There must be space for a null terminator in the buffer. */
	assert(size > 0);

	while (src[idx]) {
		if ((src[idx] & 0xfc00) == 0xd800) {
			if (src[idx + 1] && (src[idx + 1] & 0xfc00) == 0xdc00) {
				ch = 0x10000;
				ch += (src[idx] & 0x03FF) << 10;
				ch += (src[idx + 1] & 0x03FF);
				idx += 2;
			} else
				break;
		} else {
			ch = src[idx];
			idx++;
		}
		rc = chr_encode(ch, dest, &dest_off, size - 1);
		if (rc != EOK)
			break;
	}
	dest[dest_off] = '\0';
	return rc;
}

/** Convert string to UTF16 string.
 *
 * Convert string @a src to utf16 string. The output is written to the buffer
 * specified by @a dest and @a dlen. @a dlen must be non-zero and the string
 * written will always be well-formed. Surrogate pairs also supported.
 *
 * @param dest	Destination buffer.
 * @param dlen	Number of utf16 characters that fit in the destination buffer.
 * @param src	Source string.
 *
 * @return EOK, if success, an error code otherwise.
 */
errno_t str_to_utf16(uint16_t *dest, size_t dlen, const char *src)
{
	errno_t rc = EOK;
	size_t offset = 0;
	size_t idx = 0;
	char32_t c;

	assert(dlen > 0);

	while ((c = str_decode(src, &offset, STR_NO_LIMIT)) != 0) {
		if (c > 0x10000) {
			if (idx + 2 >= dlen - 1) {
				rc = EOVERFLOW;
				break;
			}
			c = (c - 0x10000);
			dest[idx] = 0xD800 | (c >> 10);
			dest[idx + 1] = 0xDC00 | (c & 0x3FF);
			idx++;
		} else {
			dest[idx] = c;
		}

		idx++;
		if (idx >= dlen - 1) {
			rc = EOVERFLOW;
			break;
		}
	}

	dest[idx] = '\0';
	return rc;
}

/** Get size of UTF-16 string.
 *
 * Get the number of words which are used by the UTF-16 string @a ustr
 * (excluding the NULL-terminator).
 *
 * @param ustr UTF-16 string to consider.
 *
 * @return Number of words used by the UTF-16 string
 *
 */
size_t utf16_wsize(const uint16_t *ustr)
{
	size_t wsize = 0;

	while (*ustr++ != 0)
		wsize++;

	return wsize;
}

/** Convert wide string to new string.
 *
 * Convert wide string @a src to string. Space for the new string is allocated
 * on the heap.
 *
 * @param src	Source wide string.
 * @return	New string.
 */
char *wstr_to_astr(const char32_t *src)
{
	char dbuf[STR_BOUNDS(1)];
	char *str;
	char32_t ch;

	size_t src_idx;
	size_t dest_off;
	size_t dest_size;

	/* Compute size of encoded string. */

	src_idx = 0;
	dest_size = 0;

	while ((ch = src[src_idx++]) != 0) {
		dest_off = 0;
		if (chr_encode(ch, dbuf, &dest_off, STR_BOUNDS(1)) != EOK)
			break;
		dest_size += dest_off;
	}

	str = malloc(dest_size + 1);
	if (str == NULL)
		return NULL;

	/* Encode string. */

	src_idx = 0;
	dest_off = 0;

	while ((ch = src[src_idx++]) != 0) {
		if (chr_encode(ch, str, &dest_off, dest_size) != EOK)
			break;
	}

	str[dest_size] = '\0';
	return str;
}

/** Convert string to wide string.
 *
 * Convert string @a src to wide string. The output is written to the
 * buffer specified by @a dest and @a dlen. @a dlen must be non-zero
 * and the wide string written will always be null-terminated.
 *
 * @param dest	Destination buffer.
 * @param dlen	Length of destination buffer (number of wchars).
 * @param src	Source string.
 */
void str_to_wstr(char32_t *dest, size_t dlen, const char *src)
{
	size_t offset;
	size_t di;
	char32_t c;

	assert(dlen > 0);

	offset = 0;
	di = 0;

	do {
		if (di >= dlen - 1)
			break;

		c = str_decode(src, &offset, STR_NO_LIMIT);
		dest[di++] = c;
	} while (c != '\0');

	dest[dlen - 1] = '\0';
}

/** Convert string to wide string.
 *
 * Convert string @a src to wide string. A new wide NULL-terminated
 * string will be allocated on the heap.
 *
 * @param src	Source string.
 */
char32_t *str_to_awstr(const char *str)
{
	size_t len = str_length(str);

	char32_t *wstr = calloc(len + 1, sizeof(char32_t));
	if (wstr == NULL)
		return NULL;

	str_to_wstr(wstr, len + 1, str);
	return wstr;
}

static char *_strchr(const char *str, char c)
{
	while (*str != 0 && *str != c)
		str++;

	return (*str == c) ? (char *) str : NULL;
}

/** Find first occurence of character in string.
 *
 * @param str String to search.
 * @param ch  Character to look for.
 *
 * @return Pointer to character in @a str or NULL if not found.
 */
char *str_chr(const char *str, char32_t ch)
{
	/* Fast path for an ASCII character. */
	if (ascii_check(ch))
		return _strchr(str, ch);

	/* Convert character to UTF-8. */
	char utf8[STR_BOUNDS(1) + 1];
	size_t offset = 0;

	if (chr_encode(ch, utf8, &offset, sizeof(utf8)) != EOK || offset == 0)
		return NULL;

	utf8[offset] = '\0';

	/* Find the first byte, then check if all of them are correct. */
	while (*str != 0) {
		str = _strchr(str, utf8[0]);
		if (!str)
			return NULL;

		if (_test_prefix(str, utf8))
			return (char *) str;

		str++;
	}

	return NULL;
}

/** Find first occurence of substring in string.
 *
 * @param hs  Haystack (string)
 * @param n   Needle (substring to look for)
 *
 * @return Pointer to character in @a hs or @c NULL if not found.
 */
char *str_str(const char *hs, const char *n)
{
	size_t hsize = _str_size(hs);
	size_t nsize = _str_size(n);

	while (hsize >= nsize) {
		if (_test_prefix(hs, n))
			return (char *) hs;

		hs++;
		hsize--;
	}

	return NULL;
}

static void _str_rtrim(char *str, char c)
{
	char *last = str;

	while (*str) {
		if (*str != c)
			last = str;

		str++;
	}

	/* Truncate string. */
	last[1] = 0;
}

/** Removes specified trailing characters from a string.
 *
 * @param str String to remove from.
 * @param ch  Character to remove.
 */
void str_rtrim(char *str, char32_t ch)
{
	/* Fast path for the ASCII case. */
	if (ascii_check(ch)) {
		_str_rtrim(str, ch);
		return;
	}

	size_t off = 0;
	size_t pos = 0;
	char32_t c;
	bool update_last_chunk = true;
	char *last_chunk = NULL;

	while ((c = str_decode(str, &off, STR_NO_LIMIT))) {
		if (c != ch) {
			update_last_chunk = true;
			last_chunk = NULL;
		} else if (update_last_chunk) {
			update_last_chunk = false;
			last_chunk = (str + pos);
		}
		pos = off;
	}

	if (last_chunk)
		*last_chunk = '\0';
}

static void _str_ltrim(char *str, char c)
{
	char *p = str;

	while (*p == c)
		p++;

	if (str != p)
		_str_cpy(str, p);
}

/** Removes specified leading characters from a string.
 *
 * @param str String to remove from.
 * @param ch  Character to remove.
 */
void str_ltrim(char *str, char32_t ch)
{
	/* Fast path for the ASCII case. */
	if (ascii_check(ch)) {
		_str_ltrim(str, ch);
		return;
	}

	char32_t acc;
	size_t off = 0;
	size_t pos = 0;
	size_t str_sz = str_size(str);

	while ((acc = str_decode(str, &off, STR_NO_LIMIT)) != 0) {
		if (acc != ch)
			break;
		else
			pos = off;
	}

	if (pos > 0) {
		memmove(str, &str[pos], str_sz - pos);
		pos = str_sz - pos;
		str[pos] = '\0';
	}
}

static char *_str_rchr(const char *str, char c)
{
	const char *last = NULL;

	while (*str) {
		if (*str == c)
			last = str;

		str++;
	}

	return (char *) last;
}

/** Find last occurence of character in string.
 *
 * @param str String to search.
 * @param ch  Character to look for.
 *
 * @return Pointer to character in @a str or NULL if not found.
 */
char *str_rchr(const char *str, char32_t ch)
{
	if (ascii_check(ch))
		return _str_rchr(str, ch);

	char32_t acc;
	size_t off = 0;
	size_t last = 0;
	const char *res = NULL;

	while ((acc = str_decode(str, &off, STR_NO_LIMIT)) != 0) {
		if (acc == ch)
			res = (str + last);
		last = off;
	}

	return (char *) res;
}

/** Insert a wide character into a wide string.
 *
 * Insert a wide character into a wide string at position
 * @a pos. The characters after the position are shifted.
 *
 * @param str     String to insert to.
 * @param ch      Character to insert to.
 * @param pos     Character index where to insert.
 * @param max_pos Characters in the buffer.
 *
 * @return True if the insertion was sucessful, false if the position
 *         is out of bounds.
 *
 */
bool wstr_linsert(char32_t *str, char32_t ch, size_t pos, size_t max_pos)
{
	size_t len = wstr_length(str);

	if ((pos > len) || (pos + 1 > max_pos))
		return false;

	size_t i;
	for (i = len; i + 1 > pos; i--)
		str[i + 1] = str[i];

	str[pos] = ch;

	return true;
}

/** Remove a wide character from a wide string.
 *
 * Remove a wide character from a wide string at position
 * @a pos. The characters after the position are shifted.
 *
 * @param str String to remove from.
 * @param pos Character index to remove.
 *
 * @return True if the removal was sucessful, false if the position
 *         is out of bounds.
 *
 */
bool wstr_remove(char32_t *str, size_t pos)
{
	size_t len = wstr_length(str);

	if (pos >= len)
		return false;

	size_t i;
	for (i = pos + 1; i <= len; i++)
		str[i - 1] = str[i];

	return true;
}

/** Duplicate string.
 *
 * Allocate a new string and copy characters from the source
 * string into it. The duplicate string is allocated via sleeping
 * malloc(), thus this function can sleep in no memory conditions.
 *
 * The allocation cannot fail and the return value is always
 * a valid pointer. The duplicate string is always a well-formed
 * null-terminated UTF-8 string, but it can differ from the source
 * string on the byte level.
 *
 * @param src Source string.
 *
 * @return Duplicate string.
 *
 */
char *str_dup(const char *src)
{
	size_t size = _str_size(src) + 1;
	char *dest = malloc(size);
	if (!dest)
		return NULL;

	memcpy(dest, src, size);
	_str_sanitize(dest, size, U_SPECIAL);
	return dest;
}

/** Duplicate string with size limit.
 *
 * Allocate a new string and copy up to @max_size bytes from the source
 * string into it. The duplicate string is allocated via sleeping
 * malloc(), thus this function can sleep in no memory conditions.
 * No more than @max_size + 1 bytes is allocated, but if the size
 * occupied by the source string is smaller than @max_size + 1,
 * less is allocated.
 *
 * The allocation cannot fail and the return value is always
 * a valid pointer. The duplicate string is always a well-formed
 * null-terminated UTF-8 string, but it can differ from the source
 * string on the byte level.
 *
 * @param src Source string.
 * @param n   Maximum number of bytes to duplicate.
 *
 * @return Duplicate string.
 *
 */
char *str_ndup(const char *src, size_t n)
{
	size_t size = _str_nsize(src, n);

	char *dest = malloc(size + 1);
	if (!dest)
		return NULL;

	memcpy(dest, src, size);
	_str_sanitize(dest, size, U_SPECIAL);
	dest[size] = 0;
	return dest;
}

/** Split string by delimiters.
 *
 * @param s             String to be tokenized. May not be NULL.
 * @param delim		String with the delimiters.
 * @param next		Variable which will receive the pointer to the
 *                      continuation of the string following the first
 *                      occurrence of any of the delimiter characters.
 *                      May be NULL.
 * @return              Pointer to the prefix of @a s before the first
 *                      delimiter character. NULL if no such prefix
 *                      exists.
 */
char *str_tok(char *s, const char *delim, char **next)
{
	char *start, *end;

	if (!s)
		return NULL;

	size_t len = str_size(s);
	size_t cur;
	size_t tmp;
	char32_t ch;

	/* Skip over leading delimiters. */
	tmp = 0;
	cur = 0;
	while ((ch = str_decode(s, &tmp, len)) && str_chr(delim, ch))
		cur = tmp;
	start = &s[cur];

	/* Skip over token characters. */
	tmp = cur;
	while ((ch = str_decode(s, &tmp, len)) && !str_chr(delim, ch))
		cur = tmp;
	end = &s[cur];
	if (next)
		*next = (ch ? &s[tmp] : &s[cur]);

	if (start == end)
		return NULL;	/* No more tokens. */

	/* Overwrite delimiter with NULL terminator. */
	*end = '\0';
	return start;
}

void order_suffix(const uint64_t val, uint64_t *rv, char *suffix)
{
	if (val > UINT64_C(10000000000000000000)) {
		*rv = val / UINT64_C(1000000000000000000);
		*suffix = 'Z';
	} else if (val > UINT64_C(1000000000000000000)) {
		*rv = val / UINT64_C(1000000000000000);
		*suffix = 'E';
	} else if (val > UINT64_C(1000000000000000)) {
		*rv = val / UINT64_C(1000000000000);
		*suffix = 'T';
	} else if (val > UINT64_C(1000000000000)) {
		*rv = val / UINT64_C(1000000000);
		*suffix = 'G';
	} else if (val > UINT64_C(1000000000)) {
		*rv = val / UINT64_C(1000000);
		*suffix = 'M';
	} else if (val > UINT64_C(1000000)) {
		*rv = val / UINT64_C(1000);
		*suffix = 'k';
	} else {
		*rv = val;
		*suffix = ' ';
	}
}

void bin_order_suffix(const uint64_t val, uint64_t *rv, const char **suffix,
    bool fixed)
{
	if (val > UINT64_C(1152921504606846976)) {
		*rv = val / UINT64_C(1125899906842624);
		*suffix = "EiB";
	} else if (val > UINT64_C(1125899906842624)) {
		*rv = val / UINT64_C(1099511627776);
		*suffix = "TiB";
	} else if (val > UINT64_C(1099511627776)) {
		*rv = val / UINT64_C(1073741824);
		*suffix = "GiB";
	} else if (val > UINT64_C(1073741824)) {
		*rv = val / UINT64_C(1048576);
		*suffix = "MiB";
	} else if (val > UINT64_C(1048576)) {
		*rv = val / UINT64_C(1024);
		*suffix = "KiB";
	} else {
		*rv = val;
		if (fixed)
			*suffix = "B  ";
		else
			*suffix = "B";
	}
}

/** @}
 */
