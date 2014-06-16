/*
 * Copyright (c) 2012 Petr Koupy
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

/** @addtogroup vlaunch
 * @{
 */
/** @file
 */

#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <malloc.h>
#include <io/pixel.h>
#include <task.h>
#include <str.h>
#include <str_error.h>
#include <loc.h>
#include <fibril_synch.h>
#include <io/pixel.h>
#include <device/led_dev.h>

#include <window.h>
#include <grid.h>
#include <button.h>
#include <label.h>
#include <canvas.h>

#include <surface.h>
#include <source.h>
#include <drawctx.h>
#include <codec/tga.h>

#include "images.h"

#define NAME  "vlaunch"

#define LOGO_WIDTH   196
#define LOGO_HEIGHT  66

#define PERIOD  1000000
#define COLORS  7

static char *winreg = NULL;
static fibril_timer_t *timer = NULL;
static list_t led_devs;

static pixel_t colors[COLORS] = {
	PIXEL(0xff, 0xff, 0x00, 0x00),
	PIXEL(0xff, 0x00, 0xff, 0x00),
	PIXEL(0xff, 0x00, 0x00, 0xff),
	PIXEL(0xff, 0xff, 0xff, 0x00),
	PIXEL(0xff, 0xff, 0x00, 0xff),
	PIXEL(0xff, 0x00, 0xff, 0xff),
	PIXEL(0xff, 0xff, 0xff, 0xff)
};

static unsigned int color = 0;

typedef struct {
	link_t link;
	service_id_t svc_id;
	async_sess_t *sess;
} led_dev_t;

static int app_launch(const char *app)
{
	printf("%s: Spawning %s %s \n", NAME, app, winreg);
	
	task_id_t id;
	int rc = task_spawnl(&id, app, app, winreg, NULL);
	if (rc != EOK) {
		printf("%s: Error spawning %s %s (%s)\n", NAME, app,
		    winreg, str_error(rc));
		return -1;
	}
	
	task_exit_t texit;
	int retval;
	rc = task_wait(id, &texit, &retval);
	if ((rc != EOK) || (texit != TASK_EXIT_NORMAL)) {
		printf("%s: Error retrieving retval from %s (%s)\n", NAME,
		    app, str_error(rc));
		return -1;
	}
	
	return retval;
}

static void on_vterm(widget_t *widget, void *data)
{
	app_launch("/app/vterm");
}

static void on_vdemo(widget_t *widget, void *data)
{
	app_launch("/app/vdemo");
}

static void on_vlaunch(widget_t *widget, void *data)
{
	app_launch("/app/vlaunch");
}

static void timer_callback(void *data)
{
	pixel_t next_color = colors[color];
	
	color++;
	if (color >= COLORS)
		color = 0;
	
	list_foreach(led_devs, link, led_dev_t, dev) {
		if (dev->sess)
			led_dev_color_set(dev->sess, next_color);
	}
	
	fibril_timer_set(timer, PERIOD, timer_callback, NULL);
}

static void loc_callback(void)
{
	category_id_t led_cat;
	int rc = loc_category_get_id("led", &led_cat, IPC_FLAG_BLOCKING);
	if (rc != EOK)
		return;
	
	service_id_t *svcs;
	size_t count;
	rc = loc_category_get_svcs(led_cat, &svcs, &count);
	if (rc != EOK)
		return;
	
	for (size_t i = 0; i < count; i++) {
		bool known = false;
		
		/* Determine whether we already know this device. */
		list_foreach(led_devs, link, led_dev_t, dev) {
			if (dev->svc_id == svcs[i]) {
				known = true;
				break;
			}
		}
		
		if (!known) {
			led_dev_t *dev = (led_dev_t *) calloc(1, sizeof(led_dev_t));
			if (!dev)
				continue;
			
			link_initialize(&dev->link);
			dev->svc_id = svcs[i];
			dev->sess = loc_service_connect(EXCHANGE_SERIALIZE, svcs[i], 0);
			
			list_append(&dev->link, &led_devs);
		}
	}
	
	// FIXME: Handle LED device removal
	
	free(svcs);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Compositor server not specified.\n");
		return 1;
	}
	
	list_initialize(&led_devs);
	int rc = loc_register_cat_change_cb(loc_callback);
	if (rc != EOK) {
		printf("Unable to register callback for device discovery.\n");
		return 1;
	}
	
	timer = fibril_timer_create();
	if (!timer) {
		printf("Unable to create timer.\n");
		return 1;
	}
	
	surface_t *logo = decode_tga((void *) helenos_tga, helenos_tga_size, 0);
	if (!logo) {
		printf("Unable to decode logo.\n");
		return 1;
	}
	
	winreg = argv[1];
	window_t *main_window = window_open(argv[1], true, true, "vlaunch");
	if (!main_window) {
		printf("Cannot open main window.\n");
		return 1;
	}
	
	pixel_t grd_bg = PIXEL(255, 255, 255, 255);
	
	pixel_t btn_bg = PIXEL(255, 255, 255, 255);
	pixel_t btn_fg = PIXEL(255, 186, 186, 186);
	pixel_t btn_text = PIXEL(255, 0, 0, 0);
	
	pixel_t lbl_bg = PIXEL(255, 255, 255, 255);
	pixel_t lbl_text = PIXEL(255, 0, 0, 0);
	
	canvas_t *logo_canvas = create_canvas(NULL, LOGO_WIDTH, LOGO_HEIGHT,
	    logo);
	label_t *lbl_caption = create_label(NULL, "Launch application:", 16,
	    lbl_bg, lbl_text);
	button_t *btn_vterm = create_button(NULL, "vterm", 16, btn_bg,
	    btn_fg, btn_text);
	button_t *btn_vdemo = create_button(NULL, "vdemo", 16, btn_bg,
	    btn_fg, btn_text);
	button_t *btn_vlaunch = create_button(NULL, "vlaunch", 16, btn_bg,
	    btn_fg, btn_text);
	grid_t *grid = create_grid(window_root(main_window), 1, 5, grd_bg);
	
	if ((!logo_canvas) || (!lbl_caption) || (!btn_vterm) ||
	    (!btn_vdemo) || (!btn_vlaunch) || (!grid)) {
		window_close(main_window);
		printf("Cannot create widgets.\n");
		return 1;
	}
	
	sig_connect(&btn_vterm->clicked, NULL, on_vterm);
	sig_connect(&btn_vdemo->clicked, NULL, on_vdemo);
	sig_connect(&btn_vlaunch->clicked, NULL, on_vlaunch);
	
	grid->add(grid, &logo_canvas->widget, 0, 0, 1, 1);
	grid->add(grid, &lbl_caption->widget, 0, 1, 1, 1);
	grid->add(grid, &btn_vterm->widget, 0, 2, 1, 1);
	grid->add(grid, &btn_vdemo->widget, 0, 3, 1, 1);
	grid->add(grid, &btn_vlaunch->widget, 0, 4, 1, 1);
	
	window_resize(main_window, 0, 0, 210, 130 + LOGO_HEIGHT,
	    WINDOW_PLACEMENT_RIGHT | WINDOW_PLACEMENT_TOP);
	window_exec(main_window);
	
	fibril_timer_set(timer, PERIOD, timer_callback, NULL);
	
	task_retval(0);
	async_manager();
	
	return 0;
}

/** @}
 */
