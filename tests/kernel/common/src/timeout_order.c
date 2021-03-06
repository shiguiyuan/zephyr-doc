/*
 * Copyright (c) 2017 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <misc/printk.h>
#include <ztest.h>

#define NUM_TIMEOUTS 3

static struct k_timer timer[NUM_TIMEOUTS];
static struct k_sem sem[NUM_TIMEOUTS];

static int results[NUM_TIMEOUTS], cur;

static void thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uintptr_t id = (uintptr_t)p1;

	k_timer_status_sync(&timer[id]);
	printk("%s %d synced on timer %d\n", __func__, id, id);

	/* no need to protect cur, all threads have the same prio */
	results[cur++] = id;

	k_sem_give(&sem[id]);
}

static __noinit __stack char stacks[NUM_TIMEOUTS][512];

void timeout_order_test(void)
{
	int ii, prio = k_thread_priority_get(k_current_get()) + 1;

	for (ii = 0; ii < NUM_TIMEOUTS; ii++) {
		(void)k_thread_spawn(stacks[ii], 512, thread,
				     (void *)ii, 0, 0, prio, 0, 0);
		k_timer_init(&timer[ii], 0, 0);
		k_sem_init(&sem[ii], 0, 1);
		results[ii] = -1;
	}


	uint32_t uptime = k_uptime_get_32();

	/* sync on tick */
	while (uptime == k_uptime_get_32())
		;

	for (ii = 0; ii < NUM_TIMEOUTS; ii++) {
		k_timer_start(&timer[ii], 100, 0);
	}

	struct k_poll_event poll_events[NUM_TIMEOUTS];

	for (ii = 0; ii < NUM_TIMEOUTS; ii++) {
		k_poll_event_init(&poll_events[ii], K_POLL_TYPE_SEM_AVAILABLE,
				  K_POLL_MODE_NOTIFY_ONLY, &sem[ii]);
	}

	/* drop prio to get all poll events together */
	k_thread_priority_set(k_current_get(), prio + 1);

	assert_equal(k_poll(poll_events, NUM_TIMEOUTS, 2000), 0, "");

	k_thread_priority_set(k_current_get(), prio - 1);

	for (ii = 0; ii < NUM_TIMEOUTS; ii++) {
		assert_equal(poll_events[ii].state,
			     K_POLL_STATE_SEM_AVAILABLE, "");
	}
	for (ii = 0; ii < NUM_TIMEOUTS; ii++) {
		assert_equal(results[ii], ii, "");
	}
}
