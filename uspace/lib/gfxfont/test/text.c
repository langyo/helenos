/*
 * Copyright (c) 2021 Jiri Svoboda
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

#include <gfx/color.h>
#include <gfx/context.h>
#include <gfx/font.h>
#include <gfx/glyph.h>
#include <gfx/text.h>
#include <gfx/typeface.h>
#include <pcut/pcut.h>
#include "../private/font.h"
#include "../private/typeface.h"
#include "../private/testgc.h"

PCUT_INIT;

PCUT_TEST_SUITE(text);

/** Test text width computation with a dummy font */
PCUT_TEST(dummy_text_width)
{
	gfx_font_props_t props;
	gfx_font_metrics_t metrics;
	gfx_typeface_t *tface;
	gfx_font_t *font;
	gfx_context_t *gc;
	gfx_coord_t width;
	test_gc_t tgc;
	errno_t rc;

	rc = gfx_context_new(&test_ops, (void *)&tgc, &gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_typeface_create(gc, &tface);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	gfx_font_props_init(&props);
	gfx_font_metrics_init(&metrics);
	rc = gfx_font_create(tface, &props, &metrics, &font);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	width = gfx_text_width(font, "Hello world!");
	PCUT_ASSERT_INT_EQUALS(0, width);

	gfx_font_close(font);
	gfx_typeface_destroy(tface);

	rc = gfx_context_delete(gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

/** Test text rendering with a dummy font */
PCUT_TEST(dummy_puttext)
{
	gfx_font_props_t props;
	gfx_font_metrics_t metrics;
	gfx_typeface_t *tface;
	gfx_font_t *font;
	gfx_context_t *gc;
	gfx_color_t *color;
	gfx_text_fmt_t fmt;
	gfx_coord2_t pos;
	test_gc_t tgc;
	errno_t rc;

	rc = gfx_context_new(&test_ops, (void *)&tgc, &gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_color_new_rgb_i16(0, 0, 0, &color);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_typeface_create(gc, &tface);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	gfx_font_props_init(&props);
	gfx_font_metrics_init(&metrics);
	rc = gfx_font_create(tface, &props, &metrics, &font);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	gfx_text_fmt_init(&fmt);
	fmt.font = font;
	fmt.color = color;
	pos.x = 0;
	pos.y = 0;

	rc = gfx_puttext(&pos, &fmt, "Hello world!");
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	gfx_font_close(font);
	gfx_typeface_destroy(tface);
	gfx_color_delete(color);

	rc = gfx_context_delete(gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

/** gfx_text_start_pos() correctly computes text start position */
PCUT_TEST(text_start_pos)
{
	gfx_font_props_t props;
	gfx_font_metrics_t metrics;
	gfx_typeface_t *tface;
	gfx_font_t *font;
	gfx_context_t *gc;
	gfx_color_t *color;
	gfx_text_fmt_t fmt;
	gfx_coord2_t pos;
	test_gc_t tgc;
	errno_t rc;

	rc = gfx_context_new(&test_ops, (void *)&tgc, &gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_color_new_rgb_i16(0, 0, 0, &color);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_typeface_create(gc, &tface);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	gfx_font_props_init(&props);
	gfx_font_metrics_init(&metrics);
	metrics.ascent = 10; // XXX
	metrics.descent = 10; // XXX
	rc = gfx_font_create(tface, &props, &metrics, &font);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	gfx_text_fmt_init(&fmt);
	fmt.font = font;
	fmt.color = color;
	pos.x = 0;
	pos.y = 0;

	rc = gfx_puttext(&pos, &fmt, "Hello world!");
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	gfx_font_close(font);
	gfx_typeface_destroy(tface);
	gfx_color_delete(color);

	rc = gfx_context_delete(gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

/** gfx_text_find_pos() finds position in text */
PCUT_TEST(text_find_pos)
{
	gfx_font_props_t props;
	gfx_font_metrics_t metrics;
	gfx_typeface_t *tface;
	gfx_font_t *font;
	gfx_glyph_metrics_t gmetrics;
	gfx_glyph_t *glyph1;
	gfx_glyph_t *glyph2;
	gfx_context_t *gc;
	gfx_text_fmt_t fmt;
	gfx_coord2_t anchor;
	gfx_coord2_t fpos;
	size_t off;
	test_gc_t tgc;
	errno_t rc;

	rc = gfx_context_new(&test_ops, (void *)&tgc, &gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_typeface_create(gc, &tface);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	gfx_font_props_init(&props);
	gfx_font_metrics_init(&metrics);
	rc = gfx_font_create(tface, &props, &metrics, &font);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	/* Need to create some glyphs with metrics */
	gfx_glyph_metrics_init(&gmetrics);
	gmetrics.advance = 10;

	rc = gfx_glyph_create(font, &gmetrics, &glyph1);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_glyph_set_pattern(glyph1, "A");
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	gfx_glyph_metrics_init(&gmetrics);
	gmetrics.advance = 1;

	rc = gfx_glyph_create(font, &gmetrics, &glyph2);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_glyph_set_pattern(glyph2, "i");
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	gfx_text_fmt_init(&fmt);
	fmt.font = font;
	anchor.x = 10;
	anchor.y = 0;

	fpos.x = 9;
	fpos.y = 0;
	off = gfx_text_find_pos(&anchor, &fmt, "Aii", &fpos);
	PCUT_ASSERT_INT_EQUALS(0, off);

	fpos.x = 10;
	fpos.y = 0;
	off = gfx_text_find_pos(&anchor, &fmt, "Aii", &fpos);
	PCUT_ASSERT_INT_EQUALS(0, off);

	fpos.x = 11;
	fpos.y = 0;
	off = gfx_text_find_pos(&anchor, &fmt, "Aii", &fpos);
	PCUT_ASSERT_INT_EQUALS(0, off);

	fpos.x = 19;
	fpos.y = 0;
	off = gfx_text_find_pos(&anchor, &fmt, "Aii", &fpos);
	PCUT_ASSERT_INT_EQUALS(1, off);

	fpos.x = 20;
	fpos.y = 0;
	off = gfx_text_find_pos(&anchor, &fmt, "Aii", &fpos);
	PCUT_ASSERT_INT_EQUALS(2, off);

	fpos.x = 21;
	fpos.y = 0;
	off = gfx_text_find_pos(&anchor, &fmt, "Aii", &fpos);
	PCUT_ASSERT_INT_EQUALS(3, off);

	fpos.x = 22;
	fpos.y = 0;
	off = gfx_text_find_pos(&anchor, &fmt, "Aii", &fpos);
	PCUT_ASSERT_INT_EQUALS(3, off);

	gfx_glyph_destroy(glyph1);
	gfx_glyph_destroy(glyph2);

	gfx_font_close(font);
	gfx_typeface_destroy(tface);

	rc = gfx_context_delete(gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

/** gfx_text_find_pos() finds position in text in text mode */
PCUT_TEST(text_find_pos_text)
{
	gfx_typeface_t *tface;
	gfx_font_t *font;
	gfx_context_t *gc;
	test_gc_t tgc;
	size_t off;
	gfx_text_fmt_t fmt;
	gfx_coord2_t anchor;
	gfx_coord2_t fpos;
	errno_t rc;

	rc = gfx_context_new(&test_ops, (void *)&tgc, &gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_typeface_create(gc, &tface);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_font_create_textmode(tface, &font);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	anchor.x = 10;
	anchor.y = 0;
	gfx_text_fmt_init(&fmt);
	fmt.font = font;

	fpos.x = 9;
	fpos.y = 0;
	off = gfx_text_find_pos(&anchor, &fmt, "Abc", &fpos);
	PCUT_ASSERT_INT_EQUALS(0, off);

	fpos.x = 10;
	fpos.y = 0;
	off = gfx_text_find_pos(&anchor, &fmt, "Abc", &fpos);
	PCUT_ASSERT_INT_EQUALS(0, off);

	fpos.x = 11;
	fpos.y = 0;
	off = gfx_text_find_pos(&anchor, &fmt, "Abc", &fpos);
	PCUT_ASSERT_INT_EQUALS(1, off);

	fpos.x = 12;
	fpos.y = 0;
	off = gfx_text_find_pos(&anchor, &fmt, "Abc", &fpos);
	PCUT_ASSERT_INT_EQUALS(2, off);

	fpos.x = 13;
	fpos.y = 0;
	off = gfx_text_find_pos(&anchor, &fmt, "Abc", &fpos);
	PCUT_ASSERT_INT_EQUALS(3, off);

	fpos.x = 14;
	fpos.y = 0;
	off = gfx_text_find_pos(&anchor, &fmt, "Abc", &fpos);
	PCUT_ASSERT_INT_EQUALS(3, off);

	gfx_font_close(font);
	gfx_typeface_destroy(tface);

	rc = gfx_context_delete(gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

/** gfx_text_cont() produces correct continuation parameters */
PCUT_TEST(text_cont)
{
	gfx_typeface_t *tface;
	gfx_font_t *font;
	gfx_context_t *gc;
	gfx_color_t *color;
	test_gc_t tgc;
	gfx_text_fmt_t fmt;
	gfx_coord2_t anchor;
	gfx_coord2_t cpos;
	gfx_text_fmt_t cfmt;
	errno_t rc;

	rc = gfx_context_new(&test_ops, (void *)&tgc, &gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_typeface_create(gc, &tface);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_font_create_textmode(tface, &font);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_color_new_rgb_i16(0, 0, 0, &color);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	anchor.x = 10;
	anchor.y = 20;
	gfx_text_fmt_init(&fmt);
	fmt.font = font;
	fmt.color = color;

	gfx_text_cont(&anchor, &fmt, "Abc", &cpos, &cfmt);

	PCUT_ASSERT_INT_EQUALS(13, cpos.x);
	PCUT_ASSERT_INT_EQUALS(20, cpos.y);
	PCUT_ASSERT_EQUALS(fmt.color, cfmt.color);
	PCUT_ASSERT_EQUALS(gfx_halign_left, cfmt.halign);
	PCUT_ASSERT_EQUALS(gfx_valign_baseline, cfmt.valign);

	gfx_font_close(font);
	gfx_typeface_destroy(tface);
	gfx_color_delete(color);

	rc = gfx_context_delete(gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

/** gfx_text_rect() computes bounding rectangle */
PCUT_TEST(text_rect)
{
	gfx_typeface_t *tface;
	gfx_font_t *font;
	gfx_context_t *gc;
	gfx_color_t *color;
	test_gc_t tgc;
	gfx_text_fmt_t fmt;
	gfx_coord2_t anchor;
	gfx_rect_t rect;
	errno_t rc;

	rc = gfx_context_new(&test_ops, (void *)&tgc, &gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_typeface_create(gc, &tface);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_font_create_textmode(tface, &font);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = gfx_color_new_rgb_i16(0, 0, 0, &color);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	anchor.x = 10;
	anchor.y = 20;
	gfx_text_fmt_init(&fmt);
	fmt.font = font;
	fmt.color = color;

	gfx_text_rect(&anchor, &fmt, "Abc", &rect);

	PCUT_ASSERT_INT_EQUALS(10, rect.p0.x);
	PCUT_ASSERT_INT_EQUALS(20, rect.p0.y);
	PCUT_ASSERT_INT_EQUALS(13, rect.p1.x);
	PCUT_ASSERT_INT_EQUALS(21, rect.p1.y);

	gfx_font_close(font);
	gfx_typeface_destroy(tface);
	gfx_color_delete(color);

	rc = gfx_context_delete(gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_EXPORT(text);
