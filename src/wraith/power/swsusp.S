.text

/* Originally gcc generated, modified by hand
 *
 * This may not use any stack, nor any variable that is not "NoSave":
 *
 * Its rewriting one kernel image with another. What is stack in "old"
 * image could very well be data page in "new" image, and overwriting
 * your own stack under you is bad idea.
 */

#include <linux/linkage.h>
#include <asm/segment.h>
#include <asm/page.h>

	.text

ENTRY(swsusp_arch_suspend)

	movl %esp, saved_context_esp
	movl %ebx, saved_context_ebx
	movl %ebp, saved_context_ebp
	movl %esi, saved_context_esi
	movl %edi, saved_context_edi
	pushfl ; popl saved_context_eflags

	call swsusp_save
	ret

ENTRY(swsusp_arch_resume)
	movl $swsusp_pg_dir-__PAGE_OFFSET,%ecx
	movl %ecx,%cr3

	movl	pagedir_nosave, %ebx
	xorl	%eax, %eax
	xorl	%edx, %edx
	.p2align 4,,7

copy_loop:
	movl	4(%ebx,%edx),%edi
	movl	(%ebx,%edx),%esi

	movl	$1024, %ecx
	rep
	movsl

	incl	%eax
	addl	$16, %edx
	cmpl	nr_copy_pages,%eax
	jb copy_loop
	.p2align 4,,7

	movl saved_context_esp, %esp
	movl saved_context_ebp, %ebp
	movl saved_context_ebx, %ebx
	movl saved_context_esi, %esi
	movl saved_context_edi, %edi

	pushl saved_context_eflags ; popfl
	call swsusp_restore
	ret
