/*
 *
 * Copyright 2014, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "src/core/iomgr/alarm.h"

#include <string.h>

#include "src/core/iomgr/alarm_internal.h"
#include <grpc/support/log.h>
#include "test/core/util/test_config.h"

#define MAX_CB 30

static int cb_called[MAX_CB][GRPC_CALLBACK_DO_NOT_USE];
static int kicks;

void grpc_kick_poller() { ++kicks; }

static void cb(void *arg, grpc_iomgr_cb_status status) {
  cb_called[(gpr_intptr)arg][status]++;
}

static void add_test() {
  gpr_timespec start = gpr_now();
  int i;
  grpc_alarm alarms[20];

  grpc_alarm_list_init(start);
  memset(cb_called, 0, sizeof(cb_called));

  /* 10 ms alarms.  will expire in the current epoch */
  for (i = 0; i < 10; i++) {
    grpc_alarm_init(&alarms[i], gpr_time_add(start, gpr_time_from_millis(10)),
                    cb, (void *)(gpr_intptr)i, start);
  }

  /* 1010 ms alarms.  will expire in the next epoch */
  for (i = 10; i < 20; i++) {
    grpc_alarm_init(&alarms[i], gpr_time_add(start, gpr_time_from_millis(1010)),
                    cb, (void *)(gpr_intptr)i, start);
  }

  /* collect alarms.  Only the first batch should be ready. */
  GPR_ASSERT(10 ==
             grpc_alarm_check(gpr_time_add(start, gpr_time_from_millis(500))));
  for (i = 0; i < 20; i++) {
    GPR_ASSERT(cb_called[i][GRPC_CALLBACK_SUCCESS] == (i < 10));
    GPR_ASSERT(cb_called[i][GRPC_CALLBACK_CANCELLED] == 0);
    GPR_ASSERT(cb_called[i][GRPC_CALLBACK_TIMED_OUT] == 0);
  }

  GPR_ASSERT(0 ==
             grpc_alarm_check(gpr_time_add(start, gpr_time_from_millis(600))));
  for (i = 0; i < 30; i++) {
    GPR_ASSERT(cb_called[i][GRPC_CALLBACK_SUCCESS] == (i < 10));
    GPR_ASSERT(cb_called[i][GRPC_CALLBACK_CANCELLED] == 0);
    GPR_ASSERT(cb_called[i][GRPC_CALLBACK_TIMED_OUT] == 0);
  }

  /* collect the rest of the alarms */
  GPR_ASSERT(10 ==
             grpc_alarm_check(gpr_time_add(start, gpr_time_from_millis(1500))));
  for (i = 0; i < 30; i++) {
    GPR_ASSERT(cb_called[i][GRPC_CALLBACK_SUCCESS] == (i < 20));
    GPR_ASSERT(cb_called[i][GRPC_CALLBACK_CANCELLED] == 0);
    GPR_ASSERT(cb_called[i][GRPC_CALLBACK_TIMED_OUT] == 0);
  }

  GPR_ASSERT(0 ==
             grpc_alarm_check(gpr_time_add(start, gpr_time_from_millis(1600))));
  for (i = 0; i < 30; i++) {
    GPR_ASSERT(cb_called[i][GRPC_CALLBACK_SUCCESS] == (i < 20));
    GPR_ASSERT(cb_called[i][GRPC_CALLBACK_CANCELLED] == 0);
    GPR_ASSERT(cb_called[i][GRPC_CALLBACK_TIMED_OUT] == 0);
  }

  grpc_alarm_list_shutdown();
}

/* Cleaning up a list with pending alarms. */
void destruction_test() {
  grpc_alarm alarms[5];

  grpc_alarm_list_init(gpr_time_0);
  memset(cb_called, 0, sizeof(cb_called));

  grpc_alarm_init(&alarms[0], gpr_time_from_millis(100), cb,
                  (void *)(gpr_intptr)0, gpr_time_0);
  grpc_alarm_init(&alarms[1], gpr_time_from_millis(3), cb,
                  (void *)(gpr_intptr)1, gpr_time_0);
  grpc_alarm_init(&alarms[2], gpr_time_from_millis(100), cb,
                  (void *)(gpr_intptr)2, gpr_time_0);
  grpc_alarm_init(&alarms[3], gpr_time_from_millis(3), cb,
                  (void *)(gpr_intptr)3, gpr_time_0);
  grpc_alarm_init(&alarms[4], gpr_time_from_millis(1), cb,
                  (void *)(gpr_intptr)4, gpr_time_0);
  GPR_ASSERT(1 == grpc_alarm_check(gpr_time_from_millis(2)));
  GPR_ASSERT(1 == cb_called[4][GRPC_CALLBACK_SUCCESS]);
  grpc_alarm_cancel(&alarms[0]);
  grpc_alarm_cancel(&alarms[3]);
  GPR_ASSERT(1 == cb_called[0][GRPC_CALLBACK_CANCELLED]);
  GPR_ASSERT(1 == cb_called[3][GRPC_CALLBACK_CANCELLED]);

  grpc_alarm_list_shutdown();
  GPR_ASSERT(1 == cb_called[1][GRPC_CALLBACK_CANCELLED]);
  GPR_ASSERT(1 == cb_called[2][GRPC_CALLBACK_CANCELLED]);
}

int main(int argc, char **argv) {
  grpc_test_init(argc, argv);
  add_test();
  destruction_test();
  return 0;
}