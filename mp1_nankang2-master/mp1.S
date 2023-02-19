# mp1.S - missile-command version
# Good luck, and have fun!

.data
	
    # Constants for accessing the fields of a struct missile, 
    # struct missile is defined in rtc.h and mp1.h

	NEXT 	  = 0
	X	  = 4
	Y	  = 8
	VX	  = 12
	VY	  = 16
	DEST_X	  = 20
	DEST_Y	  = 24
	EXPLODED  = 28
	C	  = 32

    # Character to draw for an explosion - '@'

	EXPLOSION = 64
	SPACE = 32


# Data shared between this file and rtc.c helper functions

# This '.globl' directive makes these symbols visible externally
.globl mp1_missile_list, base_alive, mp1_score

mp1_missile_list:	.long 0x0	# Head of list pointer
base_alive:		.long 0x0	# Array of 3 bytes, plus a padding byte
mp1_score:		.long 0x0	# Player's current score


# Data private to this  file

base_pic:     	    .string "/^-^\\"	# Picture of a live base
dead_base_pic: 	    .string "xxxxx"	# Picture of a dead base
crosshairs_x:		.long 0x0	# X-position of the crosshairs
crosshairs_y:		.long 0x0	# Y-position of the crosshairs

.text

# void mp1_poke(void);
# You can use this function to write to video memory.
#
# Interface: Register-based arguments (not C-style)
#    Inputs: %cl  - The byte you wish to write
#            %eax - Offset from the start of video memory that you wish
#                   to write to
#   Outputs: Text-mode video screen is written to at location %eax with
#            the byte in %cl
# Registers: Clobbers EDX

mp1_poke:
		movl    vmem_base_addr(,1),%edx
		movb    %cl,(%edx,%eax,1)
		ret

#   ----------------- Exported functions ---------------------

# void mp1_rtc_tasklet(unsigned long garbage);
# Performs three tasks:
#	(1) updates the list of missiles (implement this in update_missiles,
#           below, and call it from here).
#	(2) Redraw the bases - they may have been overwritten with missiles
#	(3) Redraw the crosshairs - it may have been overwritten with missiles
# Inputs   : none
# Outputs  : none
# Registers: Standard C calling convention


.globl mp1_rtc_tasklet
mp1_rtc_tasklet:
	  	pushl 	%ebp
  		movl 	%esp, %ebp
		pushl	%ebx
		pushl	%esi

		pushl 	%ecx
		pushl 	%edx
		call 	update_missiles
		popl	%edx
		popl	%ecx
		
		pushl	%ecx
		pushl	%edx
		call	draw_city
		popl	%edx
		popl	%ecx
		
		movl	crosshairs_x(,1), %ebx
		movl	crosshairs_y(,1), %esi
		#redraw 
		imull 	$160, %esi, %esi # 2*80*Y(roll)
		imull 	$2, %ebx, %ebx
		andl 	$0, %eax # eax <- 0
		addl 	%ebx, %eax
		addl 	%esi, %eax


		pushl 	%edx
		movb 	$43, %cl # + 43
		call mp1_poke
		popl 	%edx


		popl	%esi
		popl	%ebx
		
		leave
		ret

# int mp1_ioctl(unsigned long arg, unsigned int cmd)
# The dispatch function for the MP1 ioctls - should use the cmd argument
# and a jumptable to execute one of the specific ioctls implemented below.
# Inputs   : unsigned long arg - parameter to the mp1_ioctl_....
#          : unsigned int cmd  - specifies which mp1_ioctl_... function 
#          :                     to execute
# Outputs  : Returns an integer - depends on which ioctl() function is called
# Registers: Standard C calling convention
# ecx - store given arg
# edx - store command number


.globl mp1_ioctl
mp1_ioctl:
    movl 	8(%esp),%eax

	cmpl	$0, %eax #if command number < 0, fail
	jb		fail
	cmpl	$4, %eax
	ja		fail
	
	jmp *jmp_command(, %eax, 4)

	fail:
		movl	$-1, %eax
		ret
	
	
	jmp_command: 
		.long mp1_ioctl_startgame, mp1_ioctl_addmissile, mp1_ioctl_movexhairs, mp1_ioctl_getstatus, mp1_ioctl_endgame


# ----------------- Functions private to this file -------------------


# edi: store the head of mp1_missile_list
# edx: store the prev node, temp register, used to hold dest_x or dest_y 
# ebx : temp register, used to hold temp X value when comparing, hold temp value to calculate offset when drawing, and calculate if the screen position had change
# esi : temp register, used to hold temp value to calculate offset when drawing, and calculate position
update_missiles:
		pushl 	%ebp
  		movl 	%esp, %ebp

		pushl 	%ebx
		pushl 	%esi
		pushl	%edi
		

		movl 	mp1_missile_list, %edi #assign the head of the missile list to a register
		movl 	mp1_missile_list, %edx

	missile_loop:
		cmpl	$0, %edi #compare NEXT
		je missile_done  #if = NULL, all missiles are done
		# edx = edi edi = edi->next

		cmpl	$0, EXPLODED(%edi)
		jne		explosion
		
		#update position
		movl	VX(%edi), %esi
		addl	%esi, X(%edi)
		movl	VY(%edi), %esi
		addl	%esi, Y(%edi)

	

	# compare x
	check_in_screen:# if the missile moved off of the screen
		movl	X(%edi), %ebx
		sarl	$16, %ebx 
		cmpl	$0, %ebx
		jl out_screen #if x < 0, out of screen 
		cmpl    $79, %ebx
		jg out_screen #if x > 79, out

		movl	Y(%edi), %esi
		sarl	$16, %esi
		cmpl	$0, %esi
		jl out_screen #if y < 0, out of screen 
		cmpl    $24, %esi
		jg out_screen #if y > 24, out
		jmp reach_dest_or_cur_exploding
		# the missile is OUT_OF_SCREEN, remove it

		out_screen:
		pushl 	%edx
		pushl 	%edi
		call mp1_missile_remove
		addl 	$8, %esp
		
		jmp	missile_next
		
	
	reach_dest_or_cur_exploding:
		# compare X and DEST_X
		# 16 right shift x and dest_x
		movl	X(%edi), %ebx	
		sarl	$16, %ebx

		movl 	DEST_X(%edi), %esi
		cmpl 	%ebx, %esi
		jne  way_to_dest #if !=, move to condition 3
		
		# 16 right shift y and dest_y
		movl	Y(%edi), %ebx	
		sarl	$16, %ebx

		movl 	DEST_Y(%edi), %esi
		cmpl 	%ebx, %esi
		jne way_to_dest #if !=, move to condition 3

		explosion:
		pushl	%ecx
		pushl 	%edx
		pushl 	%edi
		call missile_explode
		addl 	$4, %esp
		popl 	%edx
		popl	%ecx
		

		cmpl	$0, %eax #explode if ret val = 0
		je no_notify

		pushl	%eax
		pushl	%edx
		pushl	%ecx
		call mp1_notify_user
		popl	%ecx
		popl	%edx
		popl	%eax

		
		no_notify:
			addl 	$-1, EXPLODED(%edi) #decrement EXPLORED	
			#erase 
			movl	X(%edi), %ebx	# x location (32bit)
			sarl	$16, %ebx	# x location (16bit)
			movl	Y(%edi), %esi	# y location (32bit)
			sarl	$16, %esi	# y location (16bit)
			imull	$160, %esi, %eax
			imull	$2, %ebx, %ebx
			addl	%ebx, %eax
			movb	$32, %cl #space
			pushl	%edx
			call	mp1_poke
			popl	%edx
			
			cmpl 	$0, EXPLODED(%edi)
			
			jg draw_explosion #if > 0, draw_explosion
			# = 0, remove it

			pushl 	%edx #prev node
			pushl 	%edi #curr node
			call mp1_missile_remove
			addl 	$8, %esp

			jmp missile_next

		draw_explosion:
			#draw EXPLOSION
			#calculate offset
			movl	X(%edi), %ebx	
			sarl	$16, %ebx
			movl	Y(%edi), %esi
			sarl	$16, %esi

			imull 	$160, %esi, %esi # 2*80*Y(roll)
			imull 	$2, %ebx, %ebx
			andl 	$0, %eax # eax <- 0
			addl 	%ebx, %eax
			addl 	%esi, %eax
			pushl	%edi
			movb 	$64, %cl #explosion char
			pushl 	%edx
			call mp1_poke
			popl 	%edx	
			popl	%edi
			
			jmp missile_next

	way_to_dest:
		#check whether the screen position has changed
		#erase from old position and re-draw it
		
		movl	X(%edi), %ebx	#current x location (32bit)
		subl	VX(%edi), %ebx
		sarl	$16, %ebx	#prev x location (16bit)
		movl	Y(%edi), %esi	#current y location (32bit)
		subl	VY(%edi), %esi
		sarl	$16, %esi	#prev y location (16bit)
		imull	$160, %esi, %eax
		imull	$2, %ebx, %ebx
		addl	%ebx, %eax
		movb	$32, %cl #space
		pushl	%edx
		call	mp1_poke
		popl	%edx

		#draw new
		movl	X(%edi), %ebx	#current x location (32bit)
		sarl	$16, %ebx #current x location (16bit)
		movl	Y(%edi), %esi	#current y location (32bit)
		sarl	$16, %esi #current y location (16bit)
		imull	$160, %esi, %eax
		imull	$2, %ebx, %ebx
		addl	%ebx, %eax
		movb	C(%edi), %cl # EXPLOSION STAR
		pushl	%edx
		call	mp1_poke
		popl	%edx
	
	missile_next:
		cmpl	$0, %edi
		je		missile_done
		movl	%edi, %edx
		movl	(%edi), %edi
		jmp missile_loop

	
	missile_done:
		popl 	%esi
		popl 	%ebx
		
		leave
		ret


mp1_missile_remove:
	#visualization
	#remove from list and free memory
	pushl 	%ebp
  	movl 	%esp, %ebp
	pushl	%esi
	
	movl 	8(%ebp), %edi #curr
 	movl 	12(%ebp), %edx #prev

	#erase from old position and re-draw it
		movl	X(%edi), %ebx	#current x location (32bit)
		subl	VX(%edi), %ebx
		sarl	$16, %ebx	#prev x location (16bit)
		movl	Y(%edi), %esi	#current y location (32bit)
		subl	VY(%edi), %esi
		sarl	$16, %esi	#prev y location (16bit)
		imull	$160, %esi, %eax
		imull	$2, %ebx, %ebx
		addl	%ebx, %eax
		movb	$32, %cl #space
		pushl	%edx
		call	mp1_poke
		popl	%edx
	
	cmpl	mp1_missile_list, %edi #check if edi is at the head
	jne in_the_mid
	at_the_head: #edx = edi
		movl	NEXT(%edi), %esi
		movl	%esi, mp1_missile_list
		jmp	free_missile
		
	in_the_mid: #edx - > next = edi
		movl	NEXT(%edi), %esi
		movl	%esi, NEXT(%edx)
		
	free_missile:
		pushl 	%eax
		pushl	%ebx
		pushl	%edx
		pushl 	%edi
		call mp1_free
		addl 	$4, %esp
		popl 	%edx
		popl	%ebx
		popl	%eax
		
		movl 	%esi, %edi #move next to curr

		popl	%esi
		
		leave
		ret
	

#redraw city
draw_city:
	pushl 	%ebp
  	movl 	%esp, %ebp

	pushl 	%ebx
	pushl 	%esi
	
	#ebx - counter of the outer loop
	#esi - counter of the inner loop
	#edx - hold temp calculation of the offset for drawing
	#initialize
	movl $3, %ebx
	
	OUTER_LOOP:
		addl	$-1, %ebx
		cmpl $0, %ebx
		jl finish #if ebx < 0, loop finish

		movl $5, %esi
		INNER_LOOP:
			addl	$-1, %esi	
			cmpl	$0, %esi
			jl OUTER_LOOP

			imull	$2, %esi, %eax# eg add 5, 4, 3, 2, 1 -> 22, 21, 20, 19, 18
			addl	$3840, %eax
			imull 	$40, %ebx, %edx # 20 * (2,1,0)
			addl	$38, %edx
			addl	%edx, %eax

			cmpb $0, base_alive(%ebx) #compare base_alive + (2,1,0)
			
			je draw_died_city # die

			draw_survived_city:
				movb	base_pic(, %esi, 1), %cl
				pushl	%edx
				call mp1_poke
				popl	%edx
				jmp INNER_LOOP

			draw_died_city:
				movb	dead_base_pic(, %esi, 1), %cl
				pushl	%edx
				call mp1_poke
				popl	%edx
				jmp INNER_LOOP

	finish:
		popl	%esi
		popl	%ebx
		leave
		ret



mp1_ioctl_startgame:
	pushl 	%ebp
    movl 	%esp,%ebp


	pushl	%edx

	movl	$0x00010101, base_alive(,1)

	movl	$0, mp1_score
	movl	$40, crosshairs_x(,1)
	movl	$12, crosshairs_y(,1)

	movl	$0, %eax

	popl	%edx

	leave
	ret

mp1_ioctl_addmissile:
	pushl 	%ebp
    movl 	%esp,%ebp

	pushl	%ecx
	pushl	%edx

	pushl 	%ebx
	pushl	%esi
	
	movl	8(%ebp), %ebx #assign arg to ebx
	pushl %ecx					#caller saved
	pushl %edx
	pushl	$36 #size of the struct
	call mp1_malloc
	addl	$4, %esp
	popl %edx
	popl %ecx

	cmpl	$0, %eax #if eax != 0, malloc successed
	je malloc_fail

	add_new_node:
		#copy user arg to newly malloc addr
		movl	%eax, %esi
		pushl %edx
		pushl %ecx
		pushl 	$36
		pushl	%ebx
		pushl	%esi
		call mp1_copy_from_user
		addl	$12, %esp
		popl %ecx
		popl %edx
		cmpl 	$0, %eax #if return value != 0, fail
		jne copy_fail

		movl	mp1_missile_list, %ebx
		movl	%ebx, NEXT(%esi)
		movl	%esi, mp1_missile_list
		movl	$0, %eax
		jmp		malloc_done
		
	
	copy_fail:
		pushl 	%eax
		pushl	%ebx
		pushl 	%ecx
		pushl 	%edx  
		pushl	%esi
		                
		pushl %esi                
		call mp1_free               
		addl $4, %esp 

		popl	%esi  
		popl 	%edx
		popl 	%ecx
		popl	%ebx
		popl 	%eax
		
	malloc_fail:
		movl 	$-1, %eax
	malloc_done:
		popl	%esi
		popl	%ebx
		popl	%edx
		popl	%ecx
		leave
		ret


mp1_ioctl_movexhairs:
	pushl 	%ebp
    movl 	%esp,%ebp

	pushl	%ecx
	pushl	%edx
	#erase old crosshair
	imull 	$160, crosshairs_y(,1), %edx # 2*80*Y(roll)
	imull 	$2, crosshairs_x(,1), %ecx
	andl 	$0, %eax # eax <- 0
	addl 	%edx, %eax
	addl 	%ecx, %eax
	pushl	%ecx
	movb 	$32, %cl # space
	pushl 	%edx
	call mp1_poke
	popl 	%edx
	popl	%ecx

	#fetch new dx, dy
	movl	8(%ebp), %ecx #dx,dy
	movl	8(%ebp), %edx #dx,dy

	sarl	$16, %edx	# dy
	shll	$16, %ecx
	sarl	$16, %ecx	#dx

	#calculate current x,y value
	addl	crosshairs_x(,1), %ecx
	addl	crosshairs_y(,1), %edx
	
	#limit the crosshair in the screen
		cmpl	$0, %ecx
		jge		compare_x_h #if x >= 0
		movl	$0, %ecx
	compare_x_h:
		cmpl	$79, %ecx
		jle		compare_y_l #if x <= 79
		movl	$79, %ecx
	compare_y_l:
		cmpl	$0, %edx #if y >= 0
		jge		compare_y_h
		movl	$0, %edx
	compare_y_h:
		cmpl	$24, %edx
		jle 	assign_xhair_val
		movl	$24, %edx
		
	assign_xhair_val:
		#update crosshair value
		movl	%ecx, crosshairs_x(,1)
		movl	%edx, crosshairs_y(,1)

	#redraw 
	imull 	$160, %edx, %edx # 2*80*Y(roll)
	imull 	$2, %ecx, %ecx
	andl 	$0, %eax # eax <- 0
	addl 	%edx, %eax
	addl 	%ecx, %eax
	pushl	%ecx #save ecx
	movb 	$43, %cl # + 43
	pushl 	%edx
	call mp1_poke
	popl 	%edx
	popl	%ecx
	
	#popl	%edx
	#popl	%ecx
	movl	$0, %eax
	popl	%edx
	popl	%ecx
	leave
	ret

mp1_ioctl_getstatus:
	pushl 	%ebp
    movl 	%esp,%ebp

	pushl	%ecx
	pushl	%edx

	#ecx - store user_status
	#edx - use to fetch city status
	pushl	%esi #esi - hold user pointer	
	
	movl	8(%ebp), %esi #pointer of 32bit int in userspace
	movl	mp1_score, %ecx #move current score to ecx

	#show cities
	andl	$0x0000ffff, %ecx #clear city status

	third_city:
	movl	$2, %edx	#the 3rd city
	cmpl 	$0, base_alive(%edx) #compare base_alive + (2,1,0)
	je second_city # die
	addl	$0x00040000, %ecx
	
	second_city:
	movl	$1, %edx	#the 2nd city
	cmpl 	$0, base_alive(%edx) #compare base_alive + (2,1,0)
	je first_city # die
	addl	$0x00020000, %ecx

	first_city:
	movl	$0, %edx	#the 3rd city
	cmpl 	$0, base_alive(%edx) #compare base_alive + (2,1,0)
	je city_status_done # die
	addl	$0x00010000, %ecx

	city_status_done:
		pushl %ecx
		movl	%esp, %edx
		
		pushl %ecx					
		pushl %edx
		pushl 	$4
		pushl	%edx #store the pointer of ecx content
		pushl	%esi
		call mp1_copy_to_user
		addl	$12, %esp
		
		popl %edx
		popl %ecx
		popl %ecx
		cmpl	$0, %eax
		jne		getstatus_fail #if != 0, fail
		# == 0, successfully
			movl	$0, %eax
			popl	%esi
			popl	%edx
			popl	%ecx
			leave	
			ret
		
		getstatus_fail:
			movl	$-1, %eax
			popl	%esi
			popl	%edx
			popl	%ecx
			leave	
			ret

mp1_ioctl_endgame:
	pushl 	%ebp
    movl 	%esp,%ebp

	pushl	%ecx
	pushl	%edx
	pushl	%esi
	
	
	# ecx - store current node
	# edx - store the next node
	movl	mp1_missile_list, %ecx
	
	end_game_traversal:
		cmpl	$0, %ecx #if node->next != NULL
		je		after_free
		movl	%ecx, %esi
		
		movl	NEXT(%ecx), %ecx #edx = ecx->next

		pushl	%eax
		pushl	%ecx
		pushl	%edx
		pushl	%esi
		call mp1_free
		addl 	$4, %esp
		popl	%edx
		popl	%ecx
		popl	%eax
		
		jmp	end_game_traversal
		
		after_free:

		movl	$0, %eax
		popl	%esi
		popl	%edx
		popl	%ecx
		leave
		ret
