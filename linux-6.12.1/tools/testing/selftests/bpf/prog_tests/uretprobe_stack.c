// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Meta Platforms, Inc. and affiliates. */

#include <test_progs.h>
#include "uretprobe_stack.skel.h"
#include "../sdt.h"

/* We set up target_1() -> target_2() -> target_3() -> target_4() -> USDT()
 * call chain, each being traced by our BPF program. On entry or return from
 * each target_*() we are capturing user stack trace and recording it in
 * global variable, so that user space part of the test can validate it.
 *
 * Note, we put each target function into a custom section to get those
 * __start_XXX/__stop_XXX symbols, generated by linker for us, which allow us
 * to know address range of those functions
 */
__attribute__((section("uprobe__target_4")))
__weak int target_4(void)
{
	STAP_PROBE1(uretprobe_stack, target, 42);
	return 42;
}

extern const void *__start_uprobe__target_4;
extern const void *__stop_uprobe__target_4;

__attribute__((section("uprobe__target_3")))
__weak int target_3(void)
{
	return target_4();
}

extern const void *__start_uprobe__target_3;
extern const void *__stop_uprobe__target_3;

__attribute__((section("uprobe__target_2")))
__weak int target_2(void)
{
	return target_3();
}

extern const void *__start_uprobe__target_2;
extern const void *__stop_uprobe__target_2;

__attribute__((section("uprobe__target_1")))
__weak int target_1(int depth)
{
	if (depth < 1)
		return 1 + target_1(depth + 1);
	else
		return target_2();
}

extern const void *__start_uprobe__target_1;
extern const void *__stop_uprobe__target_1;

extern const void *__start_uretprobe_stack_sec;
extern const void *__stop_uretprobe_stack_sec;

struct range {
	long start;
	long stop;
};

static struct range targets[] = {
	{}, /* we want target_1 to map to target[1], so need 1-based indexing */
	{ (long)&__start_uprobe__target_1, (long)&__stop_uprobe__target_1 },
	{ (long)&__start_uprobe__target_2, (long)&__stop_uprobe__target_2 },
	{ (long)&__start_uprobe__target_3, (long)&__stop_uprobe__target_3 },
	{ (long)&__start_uprobe__target_4, (long)&__stop_uprobe__target_4 },
};

static struct range caller = {
	(long)&__start_uretprobe_stack_sec,
	(long)&__stop_uretprobe_stack_sec,
};

static void validate_stack(__u64 *ips, int stack_len, int cnt, ...)
{
	int i, j;
	va_list args;

	if (!ASSERT_GT(stack_len, 0, "stack_len"))
		return;

	stack_len /= 8;

	/* check if we have enough entries to satisfy test expectations */
	if (!ASSERT_GE(stack_len, cnt, "stack_len2"))
		return;

	if (env.verbosity >= VERBOSE_NORMAL) {
		printf("caller: %#lx - %#lx\n", caller.start, caller.stop);
		for (i = 1; i < ARRAY_SIZE(targets); i++)
			printf("target_%d: %#lx - %#lx\n", i, targets[i].start, targets[i].stop);
		for (i = 0; i < stack_len; i++) {
			for (j = 1; j < ARRAY_SIZE(targets); j++) {
				if (ips[i] >= targets[j].start && ips[i] < targets[j].stop)
					break;
			}
			if (j < ARRAY_SIZE(targets)) { /* found target match */
				printf("ENTRY #%d: %#lx (in target_%d)\n", i, (long)ips[i], j);
			} else if (ips[i] >= caller.start && ips[i] < caller.stop) {
				printf("ENTRY #%d: %#lx (in caller)\n", i, (long)ips[i]);
			} else {
				printf("ENTRY #%d: %#lx\n", i, (long)ips[i]);
			}
		}
	}

	va_start(args, cnt);

	for (i = cnt - 1; i >= 0; i--) {
		/* most recent entry is the deepest target function */
		const struct range *t = va_arg(args, const struct range *);

		ASSERT_GE(ips[i], t->start, "addr_start");
		ASSERT_LT(ips[i], t->stop, "addr_stop");
	}

	va_end(args);
}

/* __weak prevents inlining */
__attribute__((section("uretprobe_stack_sec")))
__weak void test_uretprobe_stack(void)
{
	LIBBPF_OPTS(bpf_uprobe_opts, uprobe_opts);
	struct uretprobe_stack *skel;
	int err;

	skel = uretprobe_stack__open_and_load();
	if (!ASSERT_OK_PTR(skel, "skel_open"))
		return;

	err = uretprobe_stack__attach(skel);
	if (!ASSERT_OK(err, "skel_attach"))
		goto cleanup;

	/* trigger */
	ASSERT_EQ(target_1(0), 42 + 1, "trigger_return");

	/*
	 * Stacks captured on ENTRY uprobes
	 */

	/* (uprobe 1) target_1 in stack trace*/
	validate_stack(skel->bss->entry_stack1, skel->bss->entry1_len,
		       2, &caller, &targets[1]);
	/* (uprobe 1, recursed) */
	validate_stack(skel->bss->entry_stack1_recur, skel->bss->entry1_recur_len,
		       3, &caller, &targets[1], &targets[1]);
	/* (uprobe 2) caller -> target_1 -> target_1 -> target_2 */
	validate_stack(skel->bss->entry_stack2, skel->bss->entry2_len,
		       4, &caller, &targets[1], &targets[1], &targets[2]);
	/* (uprobe 3) */
	validate_stack(skel->bss->entry_stack3, skel->bss->entry3_len,
		       5, &caller, &targets[1], &targets[1], &targets[2], &targets[3]);
	/* (uprobe 4) caller -> target_1 -> target_1 -> target_2 -> target_3 -> target_4 */
	validate_stack(skel->bss->entry_stack4, skel->bss->entry4_len,
		       6, &caller, &targets[1], &targets[1], &targets[2], &targets[3], &targets[4]);

	/* (USDT): full caller -> target_1 -> target_1 -> target_2 (uretprobed)
	 *              -> target_3 -> target_4 (uretprobes) chain
	 */
	validate_stack(skel->bss->usdt_stack, skel->bss->usdt_len,
		       6, &caller, &targets[1], &targets[1], &targets[2], &targets[3], &targets[4]);

	/*
	 * Now stacks captured on the way out in EXIT uprobes
	 */

	/* (uretprobe 4) everything up to target_4, but excluding it */
	validate_stack(skel->bss->exit_stack4, skel->bss->exit4_len,
		       5, &caller, &targets[1], &targets[1], &targets[2], &targets[3]);
	/* we didn't install uretprobes on target_2 and target_3 */
	/* (uretprobe 1, recur) first target_1 call only */
	validate_stack(skel->bss->exit_stack1_recur, skel->bss->exit1_recur_len,
		       2, &caller, &targets[1]);
	/* (uretprobe 1) just a caller in the stack trace */
	validate_stack(skel->bss->exit_stack1, skel->bss->exit1_len,
		       1, &caller);

cleanup:
	uretprobe_stack__destroy(skel);
}