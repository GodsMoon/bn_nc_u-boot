/*
 * Common LCD routines for supported CPUs
 *
 * (C) Copyright 2001-2002
 * Wolfgang Denk, DENX Software Engineering -- wd@denx.de
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/************************************************************************/
/* ** HEADER FILES							*/
/************************************************************************/

#define DEBUG 

#include <config.h>
#include <common.h>
#include <command.h>
#include <version.h>
#include <stdarg.h>
#include <linux/types.h>
#include <devices.h>
#if defined(CONFIG_POST)
#include <post.h>
#endif
#include <lcd.h>
#include <watchdog.h>

#if defined(CONFIG_PXA250)
#include <asm/byteorder.h>
#endif

#if defined(CONFIG_MPC823)
#include <lcdvideo.h>
#endif

#ifdef CONFIG_LCD

/************************************************************************/
/* ** FONT DATA								*/
/************************************************************************/
#include <video_font.h>		/* Get font data, width and height	*/

/************************************************************************/
/* ** LOGO DATA								*/
/************************************************************************/
#ifdef CONFIG_LCD_LOGO
# include <bmp_logo_b.h>		/* Get logo data, width and height	*/
# include <bmp_logo_c.h>
# include <bmp_logo_l.h>
# if LCD_BPP != LCD_COLOR16
# if (CONSOLE_COLOR_WHITE >= BMP_LOGO_OFFSET)
#  error Default Color Map overlaps with Logo Color Map
# endif
# endif
#endif

DECLARE_GLOBAL_DATA_PTR;

ulong lcd_setmem (ulong addr);

void lcd_drawchars (ushort x, ushort y, uchar *str, int count);
static inline void lcd_puts_xy (ushort x, ushort y, uchar *s);
static inline void lcd_putc_xy (ushort x, ushort y, uchar  c);

static int lcd_init (void *lcdbase);

int lcd_clear (cmd_tbl_t * cmdtp, int flag, int argc, char *argv[]);
extern void lcd_ctrl_init (void *lcdbase);
extern void lcd_enable (void);



#if LCD_BPP == LCD_COLOR8
extern void lcd_setcolreg (ushort regno,
				ushort red, ushort green, ushort blue);
#endif
#if LCD_BPP == LCD_MONOCHROME
extern void lcd_initcolregs (void);
#endif

#if LCD_BPP == LCD_COLOR16
static uchar	pixel_size = 0;
static uint		pixel_line_length = 0;
#endif

static int lcd_getbgcolor (void);
static void lcd_setfgcolor (int color);
static void lcd_setbgcolor (int color);
void lcd_logo (void);
char lcd_is_enabled = 0;
extern vidinfo_t panel_info;
extern ulong calc_fbsize(void);
static ulong fb_size;
static ushort* local_fb=0;
static ushort  h,w;

#ifdef	NOT_USED_SO_FAR
static void lcd_getcolreg (ushort regno,
				ushort *red, ushort *green, ushort *blue);
static int lcd_getfgcolor (void);
#endif	/* NOT_USED_SO_FAR */

#ifdef CONFIG_3621EVT1A
static uchar    c_orient, c_font_scale;
static uchar    c_row, c_col, c_max_rows, c_max_cols;
static ushort   c_color_fg, c_color_bg;
static ushort   fb_max_x, fb_max_y;
static ulong    fb_num_pixels;

void bn_console_setcolor(ushort fg, ushort bg)
{
    c_color_fg = fg;
    c_color_bg = bg;
}

void bn_console_clear()
{
    c_row = 0;
    c_col = 0;
    lcd_clear(0,0,0,0);
}

void bn_console_stat(bn_console_info_t *inf)
{
    if(inf)
    {
        inf->c_row = c_row;
        inf->c_col = c_col;
        inf->c_max_rows = c_max_rows;
        inf->c_max_cols = c_max_cols;
        inf->c_color_fg = c_color_fg;
        inf->c_color_bg = c_color_bg;
    }
}

void bn_console_init(uchar orientation, uchar scale, ushort fg, ushort bg)
{
    fb_max_x = panel_info.vl_col;
    fb_max_y = panel_info.vl_row;
    fb_num_pixels = fb_size/pixel_size;

    c_orient = orientation;
    c_font_scale = scale;

    if(orientation == O_PORTRAIT)
    {
        c_max_cols = fb_max_y/(VIDEO_FONT_WIDTH*c_font_scale);
        c_max_rows = fb_max_x/(VIDEO_FONT_HEIGHT*c_font_scale);
    }
    else
    {
        c_max_cols = fb_max_x/(VIDEO_FONT_WIDTH*c_font_scale);
        c_max_rows = fb_max_y/(VIDEO_FONT_HEIGHT*c_font_scale);
    }

    // Since this is the only place where orientation and Font
    // scale can be set, always clear screen
    bn_console_setcolor(fg, bg);
    bn_console_clear();
}

void bn_console_setpos(uchar row, uchar col)
{
    c_row = (row>0)? ((row>c_max_rows)? c_max_rows:row):0;
    c_col = (col>0)? ((col>c_max_cols)? c_max_cols:col):0;
}

static inline void bn_console_newline(void)
{
    c_col = 0;
    if(++c_row >= c_max_rows)
        // No scrollup supported yet
        c_row = 0;
}

static inline void bn_console_back(void)
{
    if(--c_col < 0)
    {
        c_col = c_max_cols-1;
        if(--c_row < 0)
            c_row = 0;
    }
}

static inline void bn_console_setpixel(ushort x, ushort y, ushort c)
{
    ushort rx = (c_orient == O_PORTRAIT)? (y) : (x);
    ushort ry = (c_orient == O_PORTRAIT)? (fb_max_y-x) : (y);
    ushort *dest = ((ushort *)lcd_base) + rx + (ry*pixel_line_length);
    *dest = c;
}

static void bn_console_drawchar(ushort x, ushort y, uchar c)
{
    LOG_CONSOLE("bn_console_drawchar: %c [%d, %d]\n", c, x, y);
    ushort row, col, rx, ry, sy, sx;
    for(row=0; row<VIDEO_FONT_HEIGHT; row++)
    {
        sy = y + (row*c_font_scale);
        for(ry = sy; ry < (sy+c_font_scale); ry++)
        {
            uchar bits = video_fontdata[c*VIDEO_FONT_HEIGHT+row];
            for(col=0; col<VIDEO_FONT_WIDTH; col++)
            {
                sx = x + (col*c_font_scale);
                for(rx = sx; rx < (sx+c_font_scale); rx++)
                {
                    bn_console_setpixel(rx, ry,
                        (bits & 0x80)? c_color_fg:c_color_bg);
                }
                bits <<= 1;
            }
        }
    }
}

void bn_console_putc(const char c)
{
    switch (c) {
    case '\r':	c_col = 0;
                return;

    case '\n':	bn_console_newline();
                return;

	case '\t':	/* Tab (8 chars alignment) */
                c_col |=  8;
                c_col &= ~7;

                if (c_col >= c_max_cols) {
                    bn_console_newline();
                }
                return;

    case '\b':	bn_console_back();
                return;

	default:	bn_console_drawchar(c_col*c_font_scale*VIDEO_FONT_WIDTH,
				                    c_row*c_font_scale*VIDEO_FONT_HEIGHT,
                                    c);
                if (++c_col >= c_max_cols)
                    bn_console_newline();
                return;
    }
    /* NOTREACHED */
}

void bn_console_puts(const char* s)
{
    while(*s)
        bn_console_putc(*s++);
}

void bn_console_printf(const char* fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    bn_console_puts(buf);
    va_end(args);
}
#endif /* CONFIG_3621EVT1A */

/************************************************************************/

/*----------------------------------------------------------------------*/

static void console_scrollup (void)
{
#if 1
	/* Copy up rows ignoring the first one */
	memcpy (CONSOLE_ROW_FIRST, CONSOLE_ROW_SECOND, CONSOLE_SCROLL_SIZE);

	/* Clear the last one */
	memset (CONSOLE_ROW_LAST, COLOR_MASK(lcd_color_bg), CONSOLE_ROW_SIZE);
#else
	/*
	 * Poor attempt to optimize speed by moving "long"s.
	 * But the code is ugly, and not a bit faster :-(
	 */
	ulong *t = (ulong *)CONSOLE_ROW_FIRST;
	ulong *s = (ulong *)CONSOLE_ROW_SECOND;
	ulong    l = CONSOLE_SCROLL_SIZE / sizeof(ulong);
	uchar  c = lcd_color_bg & 0xFF;
	ulong val= (c<<24) | (c<<16) | (c<<8) | c;

	while (l--)
		*t++ = *s++;

	t = (ulong *)CONSOLE_ROW_LAST;
	l = CONSOLE_ROW_SIZE / sizeof(ulong);

	while (l-- > 0)
		*t++ = val;
#endif
}

/*----------------------------------------------------------------------*/

static inline void console_back (void)
{
	if (--console_col < 0) {
		console_col = CONSOLE_COLS-1 ;
		if (--console_row < 0) {
			console_row = 0;
		}
	}

	lcd_putc_xy (console_col * VIDEO_FONT_WIDTH,
		     console_row * VIDEO_FONT_HEIGHT,
		     ' ');
}

/*----------------------------------------------------------------------*/

static inline void console_newline (void)
{
	++console_row;
	console_col = 0;

	/* Check if we need to scroll the terminal */
	if (console_row >= CONSOLE_ROWS) {
		/* Scroll everything up */
		console_scrollup () ;
		--console_row;
	}
}

/*----------------------------------------------------------------------*/

void lcd_putc (const char c)
{
	if (!lcd_is_enabled) {
		serial_putc(c);
		return;
	}

	switch (c) {
	case '\r':	console_col = 0;
			return;

	case '\n':	console_newline();
			return;

	case '\t':	/* Tab (8 chars alignment) */
			console_col |=  8;
			console_col &= ~7;

			if (console_col >= CONSOLE_COLS) {
				console_newline();
			}
			return;

	case '\b':	console_back();
			return;

	default:	lcd_putc_xy (console_col * VIDEO_FONT_WIDTH,
				     console_row * VIDEO_FONT_HEIGHT,
				     c);
			if (++console_col >= CONSOLE_COLS) {
				console_newline();
			}
			return;
	}
	/* NOTREACHED */
}

/*----------------------------------------------------------------------*/

void lcd_puts (const char *s)
{
	if (!lcd_is_enabled) {
		serial_puts (s);
		return;
	}

	while (*s) {
		lcd_putc (*s++);
	}
}

/*----------------------------------------------------------------------*/

void lcd_console_setpos(short row, short col)
{
	console_row = (row>0)? ((row > CONSOLE_ROWS)? CONSOLE_ROWS:row):0;
	console_col = (col>0)? ((col > CONSOLE_COLS)? CONSOLE_COLS:col):0;
}

/*----------------------------------------------------------------------*/

void lcd_console_setcolor(int fg, int bg)
{
	lcd_color_fg = fg;
	lcd_color_bg = bg;
}

void lcd_printf(const char* fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    lcd_puts(buf);
    va_end(args);
}

/************************************************************************/
/* ** Low-Level Graphics Routines					*/
/************************************************************************/

void lcd_drawchars (ushort x, ushort y, uchar *str, int count)
{
#if LCD_BPP == LCD_COLOR16
	ushort *dest;
	ushort row;

	dest = (ushort *)lcd_base;
	dest += y*pixel_line_length + x;

    for (row=0;  row < VIDEO_FONT_HEIGHT;  ++row, dest += pixel_line_length)  {
		uchar *s = str;
		ushort *d = dest;
		int i;

		for (i=0; i<count; ++i) {
			uchar c, bits;

			c = *s++;
			bits = video_fontdata[c * VIDEO_FONT_HEIGHT + row];

			for (c=0; c<8; ++c) {
				*d++ = (bits & 0x80) ?
						lcd_color_fg : lcd_color_bg;
				bits <<= 1;
			}
		}
    }
#else

	uchar *dest;
	ushort off, row;

	dest = (uchar *)(lcd_base + y * lcd_line_length + x * (1 << LCD_BPP) / 8);
	off  = x * (1 << LCD_BPP) % 8;

	for (row=0;  row < VIDEO_FONT_HEIGHT;  ++row, dest += lcd_line_length)  {
		uchar *s = str;
		uchar *d = dest;
		int i;

#if LCD_BPP == LCD_MONOCHROME
		uchar rest = *d & -(1 << (8-off));
		uchar sym;
#endif
		for (i=0; i<count; ++i) {
			uchar c, bits;

			c = *s++;
			bits = video_fontdata[c * VIDEO_FONT_HEIGHT + row];

#if LCD_BPP == LCD_MONOCHROME
			sym  = (COLOR_MASK(lcd_color_fg) & bits) |
			       (COLOR_MASK(lcd_color_bg) & ~bits);

			*d++ = rest | (sym >> off);
			rest = sym << (8-off);
#elif LCD_BPP == LCD_COLOR8
			for (c=0; c<8; ++c) {
				*d++ = (bits & 0x80) ?
						lcd_color_fg : lcd_color_bg;
				bits <<= 1;
			}
#endif
		}
#if LCD_BPP == LCD_MONOCHROME
		*d  = rest | (*d & ((1 << (8-off)) - 1));
#endif
	}
#endif /* LCD_BPP == LCD_COLOR16 */
}

/*----------------------------------------------------------------------*/

static inline void lcd_puts_xy (ushort x, ushort y, uchar *s)
{
#if defined(CONFIG_LCD_LOGO) && !defined(CONFIG_LCD_INFO_BELOW_LOGO) \
	&& !defined(CONFIG_3621EVT1A)
	lcd_drawchars (x, y+BMP_LOGO_HEIGHT_B, s, strlen ((char *)s));
#else
	lcd_drawchars (x, y, s, strlen ((char *)s));
#endif
}

/*----------------------------------------------------------------------*/

static inline void lcd_putc_xy (ushort x, ushort y, uchar c)
{
#if defined(CONFIG_LCD_LOGO) && !defined(CONFIG_LCD_INFO_BELOW_LOGO) \
	&& !defined(CONFIG_3621EVT1A)
	lcd_drawchars (x, y+BMP_LOGO_HEIGHT_B, &c, 1);
#else
	lcd_drawchars (x, y, &c, 1);
#endif
}

/************************************************************************/
/**  Small utility to check that you got the colours right		*/
/************************************************************************/
//#define LCD_TEST_PATTERN
#ifdef LCD_TEST_PATTERN

#define	N_BLK_VERT	2
#define	N_BLK_HOR	3

static int test_colors[N_BLK_HOR*N_BLK_VERT] = {
	CONSOLE_COLOR_RED,	CONSOLE_COLOR_GREEN,	CONSOLE_COLOR_BLUE,
	0xDE7B,	CONSOLE_COLOR_MAGENTA,	CONSOLE_COLOR_CYAN,
};

static void test_pattern (void)
{
	ushort v_max  = panel_info.vl_row;
	ushort h_max  = panel_info.vl_col;
	ushort v_step = (v_max + N_BLK_VERT - 1) / N_BLK_VERT;
	ushort h_step = (h_max + N_BLK_HOR  - 1) / N_BLK_HOR;
	ushort v, h;
	ushort *pix = (ushort *)lcd_base;

	printf ("[LCD] Test Pattern: %d x %d [%d x %d]\n",
		h_max, v_max, h_step, v_step);

	/* WARNING: Code silently assumes 8bit/pixel */
	for (v=0; v<v_max; ++v) {
		uchar iy = v / v_step;
		for (h=0; h<h_max; ++h) {
			uchar ix = N_BLK_HOR * iy + (h/h_step);
			*pix++ = test_colors[ix];
		}
	}
}
#endif /* LCD_TEST_PATTERN */


/************************************************************************/
/* ** GENERIC Initialization Routines					*/
/************************************************************************/

int drv_lcd_init (void)
{
	device_t lcddev;
	int rc;

	lcd_base = (void *)(gd->fb_base);

	lcd_line_length = (panel_info.vl_col * NBITS (panel_info.vl_bpix)) / 8;
  
	lcd_init (lcd_base);		/* LCD initialization */

	/* Device initialization */
	memset (&lcddev, 0, sizeof (lcddev));

	strcpy (lcddev.name, "lcd");
	lcddev.ext   = 0;			/* No extensions */
#ifdef CONFIG_3621EVT1A
    lcddev.flags = 0; /* Use only for splash */
#else
	lcddev.flags = DEV_FLAGS_OUTPUT;	/* Output only */
#endif
	lcddev.putc  = lcd_putc;		/* 'putc' function */
	lcddev.puts  = lcd_puts;		/* 'puts' function */

	rc = device_register (&lcddev);

	return (rc == 0) ? 1 : rc;
}

/*----------------------------------------------------------------------*/
int lcd_clear (cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	uchar c;
#if LCD_BPP == LCD_MONOCHROME
	/* Setting the palette */
	lcd_initcolregs();

#elif LCD_BPP == LCD_COLOR8
	/* Setting the palette */
	lcd_setcolreg  (CONSOLE_COLOR_BLACK,       0,    0,    0);
	lcd_setcolreg  (CONSOLE_COLOR_RED,	0xFF,    0,    0);
	lcd_setcolreg  (CONSOLE_COLOR_GREEN,       0, 0xFF,    0);
	lcd_setcolreg  (CONSOLE_COLOR_YELLOW,	0xFF, 0xFF,    0);
	lcd_setcolreg  (CONSOLE_COLOR_BLUE,        0,    0, 0xFF);
	lcd_setcolreg  (CONSOLE_COLOR_MAGENTA,	0xFF,    0, 0xFF);
	lcd_setcolreg  (CONSOLE_COLOR_CYAN,	   0, 0xFF, 0xFF);
	lcd_setcolreg  (CONSOLE_COLOR_GREY,	0xAA, 0xAA, 0xAA);
	lcd_setcolreg  (CONSOLE_COLOR_WHITE,	0xFF, 0xFF, 0xFF);
#endif
//#define CFG_WHITE_ON_BLACK
#ifndef CFG_WHITE_ON_BLACK
	lcd_setfgcolor (CONSOLE_COLOR_BLACK);
	lcd_setbgcolor (CONSOLE_COLOR_WHITE);
	c = 0xff;
#else
	lcd_setfgcolor (CONSOLE_COLOR_WHITE);
	lcd_setbgcolor (CONSOLE_COLOR_BLACK);
	c = 0x00;
#endif	/* CFG_WHITE_ON_BLACK */

#ifdef	LCD_TEST_PATTERN
	test_pattern();
#else
	/* set framebuffer to background color */
	memset ((char *)lcd_base,
		c,
		fb_size);

#endif
	/*taking out the logo from DENX SW engineering*/
	/* Paint the logo and retrieve LCD base address */
	//debug ("[LCD] Drawing the logo...\n");
	//lcd_console_address = lcd_logo ();
   console_col = 0;
	console_row = 0;

	return (0);
}

U_BOOT_CMD(
	cls,	1,	1,	lcd_clear,
	"cls     - clear screen\n",
	NULL
);

/*----------------------------------------------------------------------*/

static int lcd_init (void *lcdbase)
{
	/* Initialize the lcd controller */
	debug ("[LCD] Initializing LCD framebuffer at %p\n", lcdbase);

	lcd_ctrl_init (lcdbase);
	lcd_clear (NULL, 1, 1, NULL);	/* dummy args */
#ifndef CONFIG_LCD_NOT_ENABLED_AT_INIT
	lcd_enable ();
#endif
	/* Initialize the console */
	console_col = 0;
	lcd_console_address = lcd_base;
#ifdef CONFIG_LCD_INFO_BELOW_LOGO
	console_row = 7 + BMP_LOGO_HEIGHT / VIDEO_FONT_HEIGHT;
#else
	console_row = 1;	/* leave 1 blank line below logo */
#endif

#if LCD_BPP == LCD_COLOR16
	pixel_size = NBITS(LCD_BPP)/8;
	pixel_line_length = lcd_line_length/pixel_size;
#endif

	lcd_is_enabled = 1;


	return 0;
}


/************************************************************************/
/* ** ROM capable initialization part - needed to reserve FB memory	*/
/************************************************************************/
/*
 * This is called early in the system initialization to grab memory
 * for the LCD controller.
 * Returns new address for monitor, after reserving LCD buffer memory
 *
 * Note that this is running from ROM, so no write access to global data.
 */
ulong lcd_setmem (ulong addr)
{
	ulong size;
	int line_length = (panel_info.vl_col * NBITS (panel_info.vl_bpix)) / 8;

	debug ("LCD panel info: %d x %d, %d bit/pix\n",
		panel_info.vl_col, panel_info.vl_row, NBITS (panel_info.vl_bpix) );

	size = line_length * panel_info.vl_row;

	/* Round up to nearest full page */
	size = (size + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
  fb_size =size;
	/* Allocate pages for the frame buffer. */
	addr -= size;

	debug ("Reserving %ldk for LCD Framebuffer at: %08lx\n", size>>10, addr);

	return (addr);
}

/*----------------------------------------------------------------------*/

static void lcd_setfgcolor (int color)
{
	lcd_color_fg = color;
}

/*----------------------------------------------------------------------*/

static void lcd_setbgcolor (int color)
{
	lcd_color_bg = color;
}

/*----------------------------------------------------------------------*/

#ifdef	NOT_USED_SO_FAR
static int lcd_getfgcolor (void)
{
	return lcd_color_fg;
}
#endif	/* NOT_USED_SO_FAR */

/*----------------------------------------------------------------------*/

static int lcd_getbgcolor (void)
{
	return lcd_color_bg;
}

/*----------------------------------------------------------------------*/

/************************************************************************/
/* ** Chipset depending Bitmap / Logo stuff...                          */
/************************************************************************/
#ifdef CONFIG_LCD_LOGO
/*
 which = 0    boot image
 which = 1    low battery image
 which = 2    connect your charger image
*/

void bitmap_plot (int x, int y,uchar which)
{
	ushort *cmap;
	ushort i, j,height,width,colors,offset;
	uchar *bmap;
	ushort *bpalette;
	uchar *fb;
	ushort *fb16;
	
	switch(which) {
		case 1:
			height= BMP_LOGO_HEIGHT_L;
			width=BMP_LOGO_WIDTH_L;
			colors=BMP_LOGO_COLORS_L;
			offset=BMP_LOGO_OFFSET_L;
			bmap = &bmp_logo_bitmap_L[0];
			bpalette=&bmp_logo_palette_L[0];
			break;
		case 2:
			height= BMP_LOGO_HEIGHT_C;
			width=BMP_LOGO_WIDTH_C;
			colors=BMP_LOGO_COLORS_C;
			offset=BMP_LOGO_OFFSET_C;
			bmap = &bmp_logo_bitmap_C[0];
			bpalette=&bmp_logo_palette_C[0];
			break;
		case 0:
		default:
			height= BMP_LOGO_HEIGHT_B;
			width=BMP_LOGO_WIDTH_B;
			colors=BMP_LOGO_COLORS_B;
			offset=BMP_LOGO_OFFSET_B;
			bmap = &bmp_logo_bitmap_B[0];
			bpalette=&bmp_logo_palette_B[0];
			break;
	}
	
#if defined(CONFIG_PXA250)
	struct pxafb_info *fbi = &panel_info.pxa;
#elif defined(CONFIG_MPC823)
	volatile immap_t *immr = (immap_t *) CFG_IMMR;
	volatile cpm8xx_t *cp = &(immr->im_cpm);
#endif

	debug ("Logo: width %d  height %d  colors %d  cmap %d\n",
		width, height, colors,
		sizeof(bpalette)/(sizeof(ushort)));

	
	fb   = (uchar *)(lcd_base + y * lcd_line_length + x);

	if (NBITS(panel_info.vl_bpix) < 12) {
		/* Leave room for default color map */
#if defined(CONFIG_PXA250)
		cmap = (ushort *)fbi->palette;
#elif defined(CONFIG_MPC823)
		cmap = (ushort *)&(cp->lcd_cmap[offset*sizeof(ushort)]);
#endif

		WATCHDOG_RESET();

		/* Set color map */
		for (i=0; i<(sizeof(bpalette)/(sizeof(ushort))); ++i) {
			ushort colreg = bpalette[i];
#ifdef  CFG_INVERT_COLORS
			*cmap++ = 0xffff - colreg;
#else
			*cmap++ = colreg;
#endif
		}

		WATCHDOG_RESET();

		for (i=0; i<height; ++i) {
			memcpy (fb, bmap, width);
			bmap += width;
			fb   += panel_info.vl_col;
		}
	}
	else { /* true color mode */
	   
	   
		fb16 = (ushort *)(lcd_base + y * lcd_line_length + x);
   
		for (i=0; i<height; ++i) {
			for (j=0; j<width; j++) {
				
				fb16[j] = bpalette[(bmap[j])];
				
			}
			bmap += width;
			fb16 += panel_info.vl_col;
		}
	
	  
	
	}

	WATCHDOG_RESET();
}
#endif /* CONFIG_LCD_LOGO */

/*----------------------------------------------------------------------*/
#if (CONFIG_COMMANDS & CFG_CMD_BMP) || defined(CONFIG_SPLASH_SCREEN)
/*
 * Display the BMP file located at address bmp_image.
 * Only uncompressed.
 */
int lcd_display_bitmap(ulong bmp_image, int x, int y)
{
	ushort *cmap;
	ushort i, j;
	uchar *fb;
	bmp_image_t *bmp=(bmp_image_t *)bmp_image;
	uchar *bmap;
	ushort padded_line;
	unsigned long width, height;
	unsigned colors,bpix;
	unsigned long compression;
#if defined(CONFIG_PXA250)
	struct pxafb_info *fbi = &panel_info.pxa;
#elif defined(CONFIG_MPC823)
	volatile immap_t *immr = (immap_t *) CFG_IMMR;
	volatile cpm8xx_t *cp = &(immr->im_cpm);
#endif

	if (!((bmp->header.signature[0]=='B') &&
		(bmp->header.signature[1]=='M'))) {
		printf ("Error: no valid bmp image at %lx\n", bmp_image);
		return 1;
}

	width = le32_to_cpu (bmp->header.width);
	height = le32_to_cpu (bmp->header.height);
	colors = 1<<le16_to_cpu (bmp->header.bit_count);
	compression = le32_to_cpu (bmp->header.compression);

	bpix = NBITS(panel_info.vl_bpix);

	if ((bpix != 1) && (bpix != 8)) {
		printf ("Error: %d bit/pixel mode not supported by U-Boot\n",
			bpix);
		return 1;
	}

	if (bpix != le16_to_cpu(bmp->header.bit_count)) {
		printf ("Error: %d bit/pixel mode, but BMP has %d bit/pixel\n",
			bpix,
			le16_to_cpu(bmp->header.bit_count));
		return 1;
	}

	debug ("Display-bmp: %d x %d  with %d colors\n",
		(int)width, (int)height, (int)colors);

	if (bpix==8) {
#if defined(CONFIG_PXA250)
		cmap = (ushort *)fbi->palette;
#elif defined(CONFIG_MPC823)
		cmap = (ushort *)&(cp->lcd_cmap[255*sizeof(ushort)]);
#else
# error "Don't know location of color map"
#endif

		/* Set color map */
		for (i=0; i<colors; ++i) {
			bmp_color_table_entry_t cte = bmp->color_table[i];
			ushort colreg =
				( ((cte.red)   << 8) & 0xf800) |
				( ((cte.green) << 3) & 0x07e0) |
				( ((cte.blue)  >> 3) & 0x001f) ;
#ifdef CFG_INVERT_COLORS
			*cmap = 0xffff - colreg;
#else
			*cmap = colreg;
#endif
#if defined(CONFIG_PXA250)
			cmap++;
#elif defined(CONFIG_MPC823)
			cmap--;
#endif
		}
	}

	padded_line = (width&0x3) ? ((width&~0x3)+4) : (width);
	if ((x + width)>panel_info.vl_col)
		width = panel_info.vl_col - x;
	if ((y + height)>panel_info.vl_row)
		height = panel_info.vl_row - y;

	bmap = (uchar *)bmp + le32_to_cpu (bmp->header.data_offset);
	fb   = (uchar *) (lcd_base +
		(y + height - 1) * lcd_line_length + x);
	for (i = 0; i < height; ++i) {
		WATCHDOG_RESET();
		for (j = 0; j < width ; j++)
#if defined(CONFIG_PXA250)
			*(fb++)=*(bmap++);
#elif defined(CONFIG_MPC823)
			*(fb++)=255-*(bmap++);
#endif
		bmap += (width - padded_line);
		fb   -= (width + lcd_line_length);
	}

	return (0);
}
#endif /* (CONFIG_COMMANDS & CFG_CMD_BMP) || CONFIG_SPLASH_SCREEN */

void lcd_logo (void)
{
#ifdef CONFIG_LCD_INFO
	char info[80];
	char temp[32];
#endif /* CONFIG_LCD_INFO */

#ifdef CONFIG_SPLASH_SCREEN
	char *s;
	ulong addr;
	static int do_splash = 1;

	if (do_splash && (s = getenv("splashimage")) != NULL) {
		addr = simple_strtoul(s, NULL, 16);
		do_splash = 0;

		if (lcd_display_bitmap (addr, 0, 0) == 0) {
			return ((void *)lcd_base);
		}
	}
#endif /* CONFIG_SPLASH_SCREEN */

#ifdef CONFIG_LCD_LOGO
	//bitmap_plot (800, 230);
#endif /* CONFIG_LCD_LOGO */

#ifdef CONFIG_MPC823
# ifdef CONFIG_LCD_INFO
	sprintf (info, "%s (%s - %s) ", U_BOOT_VERSION, __DATE__, __TIME__);
	lcd_drawchars (LCD_INFO_X, LCD_INFO_Y, (uchar *)info, strlen(info));

	sprintf (info, "(C) 2004 DENX Software Engineering");
	lcd_drawchars (LCD_INFO_X, LCD_INFO_Y + VIDEO_FONT_HEIGHT,
					(uchar *)info, strlen(info));

	sprintf (info, "    Wolfgang DENK, wd@denx.de");
	lcd_drawchars (LCD_INFO_X, LCD_INFO_Y + VIDEO_FONT_HEIGHT * 2,
					(uchar *)info, strlen(info));
#  ifdef CONFIG_LCD_INFO_BELOW_LOGO
	sprintf (info, "MPC823 CPU at %s MHz",
		strmhz(temp, gd->cpu_clk));
	lcd_drawchars (LCD_INFO_X, LCD_INFO_Y + VIDEO_FONT_HEIGHT * 3,
					info, strlen(info));
	sprintf (info, "  %ld MB RAM, %ld MB Flash",
		gd->ram_size >> 20,
		gd->bd->bi_flashsize >> 20 );
	lcd_drawchars (LCD_INFO_X, LCD_INFO_Y + VIDEO_FONT_HEIGHT * 4,
					info, strlen(info));
#  else
	/* leave one blank line */

	sprintf (info, "MPC823 CPU at %s MHz, %ld MB RAM, %ld MB Flash",
		strmhz(temp, gd->cpu_clk),
		gd->ram_size >> 20,
		gd->bd->bi_flashsize >> 20 );
	lcd_drawchars (LCD_INFO_X, LCD_INFO_Y + VIDEO_FONT_HEIGHT * 4,
					(uchar *)info, strlen(info));

#  endif /* CONFIG_LCD_INFO_BELOW_LOGO */
# endif /* CONFIG_LCD_INFO */
#endif /* CONFIG_MPC823 */


#if defined(CONFIG_LCD_LOGO) && !defined(CONFIG_LCD_INFO_BELOW_LOGO)
	return ((void *)((ulong)lcd_base + BMP_LOGO_HEIGHT_L * lcd_line_length));
#else
	return ((void *)lcd_base);
#endif /* CONFIG_LCD_LOGO && !CONFIG_LCD_INFO_BELOW_LOGO */
}

/************************************************************************/
/************************************************************************/

#endif /* CONFIG_LCD */
