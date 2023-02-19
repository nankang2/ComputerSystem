/* tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"

#define debug(str, ...) \
	printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)

static int tux_ioctl_init(struct tty_struct* tty);
static int tux_ioctl_set_led(struct tty_struct* tty, unsigned long arg);
static int tux_ioctl_buttons(struct tty_struct* tty, unsigned long arg);

static unsigned long buttons; // 1 if not press, R L D U C B A S
static unsigned int ack_check; // 1, can go on; 0, return
static unsigned long LED_status; 
static unsigned char LED_font[16] = {0xE7, 0x06, 0xCB, 0x8F, 0x2E, 0xAD, 0xED, 0x86, 0xEF, 0xAF, 0xEE, 0x6D, 0xE1, 0x4F, 0xE9, 0xE8};

/************************ Protocol Implementation *************************/

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in 
 * tuxctl-ld.c. It calls this function, so all warnings there apply 
 * here as well.
 */
void tuxctl_handle_packet (struct tty_struct* tty, unsigned char* packet)
{
    unsigned a, b, c, down, left;

    a = packet[0]; /* Avoid printk() sign extending the 8-bit */
    b = packet[1]; /* values when printing them. */
    c = packet[2];

    //printk("packet : %x %x %x\n", a, b, c); 
	switch (a){//a represent an opcode
		case MTCP_ACK: //when the MTC successfully completes a command
			ack_check = 1;
			return;

		case MTCP_BIOC_EVENT:
		// b: C B A S
		// c: R D L U
			down = ((c >> 2) & 1) << 5; //shift D value to 6st bit
			left = ((c >> 1) & 1) << 6; //shift L value to 5st bit
			buttons = (((c << 4)& 0x90) | down | left | (b & 0xf)); //0x90 is the bitemask that we only need R and U value
			return;
			
		case MTCP_RESET:
			if(ack_check == 0)	
				return;

			return;
		default:
			return;	
	}
}

/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/
int 
tuxctl_ioctl (struct tty_struct* tty, struct file* file, 
	      unsigned cmd, unsigned long arg)
{	
    switch (cmd) {
	case TUX_INIT:
		return tux_ioctl_init(tty);
	case TUX_BUTTONS:
		return tux_ioctl_buttons(tty, arg);
	case TUX_SET_LED:
		return tux_ioctl_set_led(tty, arg);
	case TUX_LED_ACK:
		return 0;
	case TUX_LED_REQUEST:
		return 0;
	case TUX_READ_LED:
		return 0;
	default:
	    return -EINVAL;
    }
}

/* tux_initialize()
 * DESCRIPTION: Initializes any variables asscociated with driver and return 0
 * INPUT: NONE
 * RETURN VALUE: 0
 * SIDE EFFECTS: initialize varible
 */
int tux_ioctl_init(struct tty_struct* tty){
	unsigned char op;
	ack_check = 1;
	LED_status = 0;
	buttons = 0xFFFFFFFF;

	//Put the LED display into user-mode.
	op = MTCP_LED_USR;
	tuxctl_ldisc_put(tty, &op, 1);

	//Enable Button interrupt-on-change
	op = MTCP_BIOC_ON;
	tuxctl_ldisc_put(tty, &op, 1);
	



	return 0;
}

/* tux_set_led()
 * DESCRIPTION: Set tux LED
 * INPUT: 32-bit integer that contain info about LED settings
 * RETURN VALUE: 0
 * SIDE EFFECTS:
 */
int tux_ioctl_set_led(struct tty_struct* tty, unsigned long arg){
	unsigned char display_num[4]; //nums display on LED, specified by the low 16-bits
	unsigned char leds; //status of LEDs (on:1/off:0), only low 4 bits valid
	unsigned char dp; //status of dp (on:1/off:0), only low 4 bits valid
	unsigned int i, j; //loop index
	unsigned long bitmask; 
	unsigned char LED_buffer[6]; //at most lines:opcode, led's status, 0 to 4 led numbers

	LED_status = arg;

	if(ack_check == 0){
		return -1;
	} 
	ack_check = 0;

	bitmask = 0x0F;
	for (i = 0; i < 4; i++){ //3333222211110000
		display_num[i] = (unsigned char)((arg >> (i * 4)) & bitmask);
	}
	// printk("%u \n", display_num[0]);
	// printk("%u \n", display_num[1]);
	// printk("%u \n", display_num[2]);
	// printk("%u \n", display_num[3]);

	leds = (unsigned char)((arg >> 16) & bitmask); //led status of : LED3 LED2 LED1 LED0
	dp = (unsigned char)((arg >> 24) & bitmask);
	
	LED_buffer[0] = MTCP_LED_SET;
	LED_buffer[1] = 0xff;

	bitmask = 0x1;
	i = 2; // loop index of buffer
	for (j = 0; j < 4; j++){
		if ((leds & bitmask) != 0){ //if that LED has value
			if ((dp & bitmask) != 0){ //check if it has decimal point
				LED_buffer[i] = LED_font[display_num[j]] + 16; //add decimal point
			}
			else{
				LED_buffer[i] = LED_font[display_num[j]];
			}
		}
		else{ //if that LED doesn't have value, set it to 0
			LED_buffer[i] = 0x00;
		}
		i++;
		bitmask = bitmask << 1;
	}

	tuxctl_ldisc_put(tty, LED_buffer, 6);

	return 0;
}

/* tux_buttons()
 * DESCRIPTION: Set tux buttons
 * INPUT: a pointer to a 32-bit integer that contain info about buttons
 * RETURN VALUE: 0, if successed; -EINVAL, if fail
 * SIDE EFFECTS:
 */
int tux_ioctl_buttons(struct tty_struct* tty, unsigned long arg){
	int res;
	unsigned long* button_ptr;

	button_ptr= &buttons;

	res = copy_to_user((unsigned long*)arg, &buttons, 1);

	if (res != 0){
		return -EINVAL;
	}
	return 0;
}

