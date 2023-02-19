/*									tab:8
 *
 * photo.c - photo display functions
 *
 * "Copyright (c) 2011 by Steven S. Lumetta."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 * 
 * IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF ILLINOIS BE LIABLE TO 
 * ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL 
 * DAMAGES ARISING OUT  OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, 
 * EVEN IF THE AUTHOR AND/OR THE UNIVERSITY OF ILLINOIS HAS BEEN ADVISED 
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHOR AND THE UNIVERSITY OF ILLINOIS SPECIFICALLY DISCLAIM ANY 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE 
 * PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND NEITHER THE AUTHOR NOR
 * THE UNIVERSITY OF ILLINOIS HAS ANY OBLIGATION TO PROVIDE MAINTENANCE, 
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 * Author:	    Steve Lumetta
 * Version:	    3
 * Creation Date:   Fri Sep  9 21:44:10 2011
 * Filename:	    photo.c
 * History:
 *	SL	1	Fri Sep  9 21:44:10 2011
 *		First written (based on mazegame code).
 *	SL	2	Sun Sep 11 14:57:59 2011
 *		Completed initial implementation of functions.
 *	SL	3	Wed Sep 14 21:49:44 2011
 *		Cleaned up code for distribution.
 */


#include <string.h>

#include "assert.h"
#include "modex.h"
#include "photo.h"
#include "photo_headers.h"
#include "world.h"



/* types local to this file (declared in types.h) */

/* 
 * A room photo.  Note that you must write the code that selects the
 * optimized palette colors and fills in the pixel data using them as 
 * well as the code that sets up the VGA to make use of these colors.
 * Pixel data are stored as one-byte values starting from the upper
 * left and traversing the top row before returning to the left of
 * the second row, and so forth.  No padding should be used.
 */
struct photo_t {
    photo_header_t hdr;			/* defines height and width */
    uint8_t        palette[192][3];     /* optimized palette colors */
    uint8_t*       img;                 /* pixel data               */
};

/* 
 * An object image.  The code for managing these images has been given
 * to you.  The data are simply loaded from a file, where they have 
 * been stored as 2:2:2-bit RGB values (one byte each), including 
 * transparent pixels (value OBJ_CLR_TRANSP).  As with the room photos, 
 * pixel data are stored as one-byte values starting from the upper 
 * left and traversing the top row before returning to the left of the 
 * second row, and so forth.  No padding is used.
 */
struct image_t {
    photo_header_t hdr;			/* defines height and width */
    uint8_t*       img;                 /* pixel data               */
};


/* file-scope variables */

/* 
 * The room currently shown on the screen.  This value is not known to 
 * the mode X code, but is needed when filling buffers in callbacks from 
 * that code (fill_horiz_buffer/fill_vert_buffer).  The value is set 
 * by calling prep_room.
 */
static const room_t* cur_room = NULL; 


/* 
 * fill_horiz_buffer
 *   DESCRIPTION: Given the (x,y) map pixel coordinate of the leftmost 
 *                pixel of a line to be drawn on the screen, this routine 
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *
 *                Note that this routine draws both the room photo and
 *                the objects in the room.
 *
 *   INPUTS: (x,y) -- leftmost pixel of line to be drawn 
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void
fill_horiz_buffer (int x, int y, unsigned char buf[SCROLL_X_DIM])
{
    int            idx;   /* loop index over pixels in the line          */ 
    object_t*      obj;   /* loop index over objects in the current room */
    int            imgx;  /* loop index over pixels in object image      */ 
    int            yoff;  /* y offset into object image                  */ 
    uint8_t        pixel; /* pixel from object image                     */
    const photo_t* view;  /* room photo                                  */
    int32_t        obj_x; /* object x position                           */
    int32_t        obj_y; /* object y position                           */
    const image_t* img;   /* object image                                */

    /* Get pointer to current photo of current room. */
    view = room_photo (cur_room);

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_X_DIM; idx++) {
        buf[idx] = (0 <= x + idx && view->hdr.width > x + idx ?
		    view->img[view->hdr.width * y + x + idx] : 0);
    }

    /* Loop over objects in the current room. */
    for (obj = room_contents_iterate (cur_room); NULL != obj;
    	 obj = obj_next (obj)) {
	obj_x = obj_get_x (obj);
	obj_y = obj_get_y (obj);
	img = obj_image (obj);

        /* Is object outside of the line we're drawing? */
	if (y < obj_y || y >= obj_y + img->hdr.height ||
	    x + SCROLL_X_DIM <= obj_x || x >= obj_x + img->hdr.width) {
	    continue;
	}

	/* The y offset of drawing is fixed. */
	yoff = (y - obj_y) * img->hdr.width;

	/* 
	 * The x offsets depend on whether the object starts to the left
	 * or to the right of the starting point for the line being drawn.
	 */
	if (x <= obj_x) {
	    idx = obj_x - x;
	    imgx = 0;
	} else {
	    idx = 0;
	    imgx = x - obj_x;
	}

	/* Copy the object's pixel data. */
	for (; SCROLL_X_DIM > idx && img->hdr.width > imgx; idx++, imgx++) {
	    pixel = img->img[yoff + imgx];

	    /* Don't copy transparent pixels. */
	    if (OBJ_CLR_TRANSP != pixel) {
		buf[idx] = pixel;
	    }
	}
    }
}


/* 
 * fill_vert_buffer
 *   DESCRIPTION: Given the (x,y) map pixel coordinate of the top pixel of 
 *                a vertical line to be drawn on the screen, this routine 
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *
 *                Note that this routine draws both the room photo and
 *                the objects in the room.
 *
 *   INPUTS: (x,y) -- top pixel of line to be drawn 
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void
fill_vert_buffer (int x, int y, unsigned char buf[SCROLL_Y_DIM])
{
    int            idx;   /* loop index over pixels in the line          */ 
    object_t*      obj;   /* loop index over objects in the current room */
    int            imgy;  /* loop index over pixels in object image      */ 
    int            xoff;  /* x offset into object image                  */ 
    uint8_t        pixel; /* pixel from object image                     */
    const photo_t* view;  /* room photo                                  */
    int32_t        obj_x; /* object x position                           */
    int32_t        obj_y; /* object y position                           */
    const image_t* img;   /* object image                                */

    /* Get pointer to current photo of current room. */
    view = room_photo (cur_room);

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_Y_DIM; idx++) {
        buf[idx] = (0 <= y + idx && view->hdr.height > y + idx ?
		    view->img[view->hdr.width * (y + idx) + x] : 0);
    }

    /* Loop over objects in the current room. */
    for (obj = room_contents_iterate (cur_room); NULL != obj;
    	 obj = obj_next (obj)) {
	obj_x = obj_get_x (obj);
	obj_y = obj_get_y (obj);
	img = obj_image (obj);

        /* Is object outside of the line we're drawing? */
	if (x < obj_x || x >= obj_x + img->hdr.width ||
	    y + SCROLL_Y_DIM <= obj_y || y >= obj_y + img->hdr.height) {
	    continue;
	}

	/* The x offset of drawing is fixed. */
	xoff = x - obj_x;

	/* 
	 * The y offsets depend on whether the object starts below or 
	 * above the starting point for the line being drawn.
	 */
	if (y <= obj_y) {
	    idx = obj_y - y;
	    imgy = 0;
	} else {
	    idx = 0;
	    imgy = y - obj_y;
	}

	/* Copy the object's pixel data. */
	for (; SCROLL_Y_DIM > idx && img->hdr.height > imgy; idx++, imgy++) {
	    pixel = img->img[xoff + img->hdr.width * imgy];

	    /* Don't copy transparent pixels. */
	    if (OBJ_CLR_TRANSP != pixel) {
		buf[idx] = pixel;
	    }
	}
    }
}


/* 
 * image_height
 *   DESCRIPTION: Get height of object image in pixels.
 *   INPUTS: im -- object image pointer
 *   OUTPUTS: none
 *   RETURN VALUE: height of object image im in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
image_height (const image_t* im)
{
    return im->hdr.height;
}


/* 
 * image_width
 *   DESCRIPTION: Get width of object image in pixels.
 *   INPUTS: im -- object image pointer
 *   OUTPUTS: none
 *   RETURN VALUE: width of object image im in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
image_width (const image_t* im)
{
    return im->hdr.width;
}

/* 
 * photo_height
 *   DESCRIPTION: Get height of room photo in pixels.
 *   INPUTS: p -- room photo pointer
 *   OUTPUTS: none
 *   RETURN VALUE: height of room photo p in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
photo_height (const photo_t* p)
{
    return p->hdr.height;
}


/* 
 * photo_width
 *   DESCRIPTION: Get width of room photo in pixels.
 *   INPUTS: p -- room photo pointer
 *   OUTPUTS: none
 *   RETURN VALUE: width of room photo p in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
photo_width (const photo_t* p)
{
    return p->hdr.width;
}


/* 
 * prep_room
 *   DESCRIPTION: Prepare a new room for display.  You might want to set
 *                up the VGA palette registers according to the color
 *                palette that you chose for this room.
 *   INPUTS: r -- pointer to the new room
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: changes recorded cur_room for this file
 */
void
prep_room (const room_t* r)
{
	set_palette(room_photo(r)->palette);
    /* Record the current room. */
    cur_room = r;
}


/* 
 * read_obj_image
 *   DESCRIPTION: Read size and pixel data in 2:2:2 RGB format from a
 *                photo file and create an image structure from it.
 *   INPUTS: fname -- file name for input
 *   OUTPUTS: none
 *   RETURN VALUE: pointer to newly allocated photo on success, or NULL
 *                 on failure
 *   SIDE EFFECTS: dynamically allocates memory for the image
 */
image_t*
read_obj_image (const char* fname)
{
    FILE*    in;		/* input file               */
    image_t* img = NULL;	/* image structure          */
    uint16_t x;			/* index over image columns */
    uint16_t y;			/* index over image rows    */
    uint8_t  pixel;		/* one pixel from the file  */

    /* 
     * Open the file, allocate the structure, read the header, do some
     * sanity checks on it, and allocate space to hold the image pixels.
     * If anything fails, clean up as necessary and return NULL.
     */
    if (NULL == (in = fopen (fname, "r+b")) ||
	NULL == (img = malloc (sizeof (*img))) ||
	NULL != (img->img = NULL) || /* false clause for initialization */
	1 != fread (&img->hdr, sizeof (img->hdr), 1, in) ||
	MAX_OBJECT_WIDTH < img->hdr.width ||
	MAX_OBJECT_HEIGHT < img->hdr.height ||
	NULL == (img->img = malloc 
		 (img->hdr.width * img->hdr.height * sizeof (img->img[0])))) {
	if (NULL != img) {
	    if (NULL != img->img) {
	        free (img->img);
	    }
	    free (img);
	}
	if (NULL != in) {
	    (void)fclose (in);
	}
	return NULL;
    }

    /* 
     * Loop over rows from bottom to top.  Note that the file is stored
     * in this order, whereas in memory we store the data in the reverse
     * order (top to bottom).
     */
    for (y = img->hdr.height; y-- > 0; ) {

	/* Loop over columns from left to right. */
	for (x = 0; img->hdr.width > x; x++) {

	    /* 
	     * Try to read one 8-bit pixel.  On failure, clean up and 
	     * return NULL.
	     */
	    if (1 != fread (&pixel, sizeof (pixel), 1, in)) {
		free (img->img);
		free (img);
	        (void)fclose (in);
		return NULL;
	    }

	    /* Store the pixel in the image data. */
	    img->img[img->hdr.width * y + x] = pixel;
	}
    }

    /* All done.  Return success. */
    (void)fclose (in);
    return img;
}


/* 
 * read_photo
 *   DESCRIPTION: Read size and pixel data in 5:6:5 RGB format from a
 *                photo file and create a photo structure from it.
 *                Code provided simply maps to 2:2:2 RGB.  You must
 *                replace this code with palette color selection, and
 *                must map the image pixels into the palette colors that
 *                you have defined.
 *   INPUTS: fname -- file name for input
 *   OUTPUTS: none
 *   RETURN VALUE: pointer to newly allocated photo on success, or NULL
 *                 on failure
 *   SIDE EFFECTS: dynamically allocates memory for the photo
 */
photo_t*
read_photo (const char* fname)
{
    FILE*    in;	/* input file               */
    photo_t* p = NULL;	/* photo structure          */
    uint16_t x;		/* index over image columns */
    uint16_t y;		/* index over image rows    */
    uint16_t pixel;	/* one pixel from the file  */

    /* 
     * Open the file, allocate the structure, read the header, do some
     * sanity checks on it, and allocate space to hold the photo pixels.
     * If anything fails, clean up as necessary and return NULL.
     */
    if (NULL == (in = fopen (fname, "r+b")) ||
	NULL == (p = malloc (sizeof (*p))) ||
	NULL != (p->img = NULL) || /* false clause for initialization */
	1 != fread (&p->hdr, sizeof (p->hdr), 1, in) ||
	MAX_PHOTO_WIDTH < p->hdr.width ||
	MAX_PHOTO_HEIGHT < p->hdr.height ||
	NULL == (p->img = malloc(p->hdr.width * p->hdr.height * sizeof (p->img[0])))) {
	if (NULL != p) {
	    if (NULL != p->img) {
	        free (p->img);
	    }
	    free (p);
	}
	if (NULL != in) {
	    (void)fclose (in);
	}
	return NULL;
    }
    
	octree(p, in); //use octree to fill in palette

    fseek(in, sizeof(p->hdr), SEEK_SET); //replace the pointer at file beginning

    /* 
     * Loop over rows from bottom to top.  Note that the file is stored
     * in this order, whereas in memory we store the data in the reverse
     * order (top to bottom).
     */
    for (y = p->hdr.height; y-- > 0; ) {

	/* Loop over columns from left to right. */
	for (x = 0; p->hdr.width > x; x++) {

	    /* 
	     * Try to read one 16-bit pixel.  On failure, clean up and 
	     * return NULL.
	     */
	    if (1 != fread (&pixel, sizeof (pixel), 1, in)) {
		free (p->img);
		free (p);
	        (void)fclose (in);
		return NULL;

	    }
	    /* 
	     * 16-bit pixel is coded as 5:6:5 RGB (5 bits red, 6 bits green,
	     * and 6 bits blue).  We change to 2:2:2, which we've set for the
	     * game objects.  You need to use the other 192 palette colors
	     * to specialize the appearance of each photo.
	     *
	     * In this code, you need to calculate the p->palette values,
	     * which encode 6-bit RGB as arrays of three uint8_t's.  When
	     * the game puts up a photo, you should then change the palette 
	     * to match the colors needed for that photo.
	     */
	    p->img[p->hdr.width * y + x] = find_palette_index(p, pixel);
	}
    }

    /* All done.  Return success. */

	(void)fclose (in);
    return p;
}

/* cmpfunc
 * DESCRIPTION: help function that used to compare the frequency of the color
 * INPUT: a, b - the point of the color in the color_count table
 * RETURN VALUE: difference of the count
 * SIDE EFFECT: NONE
 */
int cmpfunc (const void * a, const void * b) {
	return(*(int*)b - *(int*)a);
}

/* octree
 * DESCRIPTION: Find other 192 colors that best represent the content of an individual room photo
 * INPUT: photo - the photo of the room
 * OUTPUT: None
 * RETURN VALUE: None
 * SIDE EFFECT: add color to the palette of the photo
 */
void
octree (photo_t* p, FILE* input_file){
    uint16_t x;		/* index over image columns */
    uint16_t y;		/* index over image rows    */ 
    uint16_t i,j;		/* index over color tabel */
    uint32_t color_count[4096][5]; //a table used to count level-4 color, 4096 = 8^4, number of 4 level color
    uint32_t level2_color[64][4]; //a table used to calculate the last 3 or 4 digits of level-2 colors
    uint16_t pixel;	/* one pixel from the file  */
    //uint16_t pixel_2; /* one pixel of 12 bits: RRRRGGGGBBBB */
    uint16_t MSB_1; /* the value of 1st MSB (range: 0-8) */
    uint16_t MSB_2; /* the value of 2nd MSB (range: 0-8) */
    uint16_t MSB_3; /* the value of 3rd MSB (range: 0-8) */
    uint16_t MSB_4; /* the value of 4st MSB (range: 0-8) */
    uint16_t offset; /* the index of the color in the table. */
    uint8_t R_5; /* 5 bits of Red */
    uint8_t G_6; /* 6 bits of Green */
    uint8_t B_5; /* 5 bits of Blue */

    //uint8_t pal[192][3];

    //initialize the color count table

    for (MSB_1 = 0; MSB_1 < 8; MSB_1++){
        for (MSB_2 = 0; MSB_2 < 8; MSB_2++){
            for (MSB_3 = 0; MSB_3 < 8; MSB_3++){
                for (MSB_4 = 0; MSB_4 < 8; MSB_4++){
                    for (j = 0; j < 4; j++){
                       color_count[MSB_1 * 512 + MSB_2 * 64 + MSB_3 * 8 + MSB_4][j] = 0; 
                    }
                    color_count[MSB_1 * 512 + MSB_2 * 64 + MSB_3 * 8 + MSB_4][4] = ((MSB_1<< 9) | (MSB_2 << 6) | (MSB_3 << 3) | (MSB_4));
                }
            }
        }
    }
    /* seperated by MSB: 0 0 0 0 1 1 1 1 
                         0 0 1 1 0 0 1 1
                         0 1 0 1 0 1 0 1
    for each color count:
        1st : the count of pixel that appear in the img
        2nd : the total Red pixel value of last 1 digit
        3rd : the total Green pixel value of last 2 digits
        4st : the total Blue pixel value of last 1 digit
        5st : the info of RGB RGB RGB RGB
    */
    

    for(i = 0; i < 64; i++){
		for (j = 0; j < 4; j++){
			level2_color[i][j] = 0;
		}
    } 
    // for each color level2_color[i] = {LSB_R, LSB_G, LSB_B, count}

	
    //Traversal through all pixels
    for (y = p->hdr.height; y-- > 0;){
        for (x = 0; p->hdr.width > x; x++){
            //try to read one 16-bit pixel
			if (1 != fread (&pixel, sizeof (pixel), 1, input_file)){return;}
	
            MSB_1 = ((((pixel >> 15) &0x1) << 2) |
					 (((pixel >> 10) & 0x1) << 1) |
					 ((pixel >> 4) & 0x1));

            MSB_2 = ((((pixel >> 14)& 0x1) << 2) |
					 (((pixel >> 9) & 0x1) << 1) |
					 ((pixel >> 3) & 0x1));
                    
            MSB_3 = ((((pixel >> 13)& 0x1) << 2) |
					 (((pixel >> 8) & 0x1) << 1) |
					 ((pixel >> 2) & 0x1));
                
            MSB_4 = ((((pixel >> 12)& 0x1) << 2) |
					 (((pixel >> 7) & 0x1) << 1) |
					 ((pixel >> 1) & 0x1));
            
            offset = MSB_1 * 512 + MSB_2 * 64 + MSB_3 * 8 + MSB_4; // 8^3 = 512, 8^2 = 64, 8^1 = 8
            color_count[offset][0] ++; //count + 1
            color_count[offset][1] += pixel >> 11; //add the 5 bits of the Red part
            color_count[offset][2] += ((pixel >> 5)& 0x3F); //add 6 bits of the Green part
            color_count[offset][3] += (pixel & 0x1F); //add the 5 bits of the Blue part
        }
    }

    //sort the color count table
    qsort(color_count, 4096, sizeof(color_count[0]), cmpfunc);

    //deal with leve-4 node:the first 128 colors
    for (i = 0; i < 128; i++){
        //calculate the average RGB value
        R_5 = (color_count[i][1] / color_count[i][0]);
        G_6 = (color_count[i][2] / color_count[i][0]);
        B_5 = (color_count[i][3] / color_count[i][0]);

        //assign color for palette[64: 191]
        p->palette[i][0] = (uint8_t)(R_5) << 1;
		p->palette[i][1] = (uint8_t)(G_6);
		p->palette[i][2] = (uint8_t)(B_5) << 1;
    }
	

    //deal with level 2 node
    //add total value of RGB LSB to level2_color table
    for (i = 128; i < 4096; i++){
        MSB_1 = (color_count[i][4] >> 9);

        MSB_2 = ((color_count[i][4] >> 6)& 0x7); //7 = 111(binary)

        offset = MSB_1 * 8 + MSB_2; //index of the color in level-2 color node
        level2_color[offset][0] +=  color_count[i][1]; //total value of R 
        
        level2_color[offset][1] +=  color_count[i][2]; //total value of G 

        level2_color[offset][2] +=  color_count[i][3]; //total value of B

        level2_color[offset][3] += color_count[i][0]; //renew count
    }

    //calculate RGB
    for (i = 0; i < 64; i++){

            if (level2_color[i][3] != 0){
                level2_color[i][0] /= level2_color[i][3];
                level2_color[i][1] /= level2_color[i][3];
                level2_color[i][2] /= level2_color[i][3]; 
                
            }
            else{
                level2_color[i][0] = 0;
                level2_color[i][1] = 0;
                level2_color[i][2] = 0;
            }
            
            R_5 = (uint8_t)(level2_color[i][0]);
            G_6 = (uint8_t)(level2_color[i][1]);
            B_5 = (uint8_t)(level2_color[i][2]); 

            p->palette[i + 128][0] = R_5 << 1; //add 128, because level2 color store after 128 level4 color
			p->palette[i + 128][1] = G_6;
			p->palette[i + 128][2] = B_5 << 1;
        
    }



}

/* find_palette_inde
 * DESCRIPTION: help function that return an index of a color in the palette
 * INPUT: p - the point of the room photo
 *        pixel - the RGB color of the pixel
 * RETURN VALUE: an index of a color in the palette
 * SIDE EFFECT: NONE
 */
uint8_t find_palette_index(photo_t* p, uint16_t pixel){
	
    int i; //index to loop the palette
    uint8_t x,y; //use to calculate the index of level-2
    uint8_t R; //R part of the pixel
    uint8_t G; //G part of the pixel
    uint8_t B; //B part of the pixel

    //get the first 4 bits for each R,G,B
    R = (uint8_t)(pixel >> 12);
    G = (uint8_t)(pixel >> 7) & 0xF;
    B = (uint8_t)(pixel >> 1) & 0xF; 
    
    
    for (i = 0; i < 128; i++){
        //if find the color in the level-4 node
        if (((p->palette[i][0] >> 2) == R)&&((p->palette[i][1] >> 2) == G)&&((p->palette[i][2] >> 2) == B)){
            return i + 64;// 64 text palette color
        }
    }
    
    R = (uint8_t)(pixel >> 14);
    G = (uint8_t)((pixel >> 9) & 0x3);
    B = (uint8_t)((pixel >> 3) & 0x3); 

    //if didn't find the color in the level-4 node, assign a level-2 color to it
    x = (R >> 1)*4 + (G >> 1)*2 + (B >> 1);
    y = (R & 0x1)*4 + (G & 0x1)*2 + (B  & 0x1);

    return (x * 8 + y + 192); //192 = 64(text palette) + 128(level4)

}


