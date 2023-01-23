// Copyright (c) 2023, Kry10 Limited. All rights reserved.
//
// SPDX-License-Identifier: LicenseRef-Kry10

#include <kos.h>

#include "ehrpwm.h"

#define VISUALIZE_STARTUP

#define EXPECTED_ARGC 2
#define PROTOCOL_NAME_IDX 1

#define PWM_REQUEST_LABEL (~0)
#define PWM_PROTOCOL_BADGE (KOS_CORE_APP_ID_LIMIT)

#define RECEIVE_TOKEN_SLOT 1
#define TRANSFER_TOKEN_SLOT 2

#define AM335X_PWM0_PADDR 0x48300000
#define AM335X_PWM1_PADDR 0x48302000
#define AM335X_PWM2_PADDR 0x48304000
#define NUM_PWM 3

#define MODULE_CLK 100000000
#define TB_CLK 100000000

#define PIN_A 0
#define PIN_B 1
#define NUM_PINS 2

#define SET_PWM_FREQUENCY_ARGS 1
#define SET_PWM_DUTY_CYCLE_ARGS 2

static kos_msg_server_t server;
static kos_thread_mgr_entry_t root_manager_entry;
static kos_thread_mgr_t root_thread_mgr;
static kos_thread_t listener_thread;

static char* protocol_name;

// ID of the only client that can be registered with us, only support one client
seL4_Word client_id;

enum request_label {
  SET_PWM_FREQUENCY_REQUEST = 1,
  SET_PWM_DUTY_CYCLE_REQUEST,
  NUM_PWM_REQUESTS
};

static kos_device_frame_t pwm_controller_frames[] = {
  {.paddr = AM335X_PWM0_PADDR, .size = KOS_EXP2(seL4_PageBits)},
  {.paddr = AM335X_PWM1_PADDR, .size = KOS_EXP2(seL4_PageBits)},
  {.paddr = AM335X_PWM2_PADDR, .size = KOS_EXP2(seL4_PageBits)}
};

static seL4_Word pwm_controller_base;
static uint32_t curr_freq;
static uint32_t curr_pin_duty_cycle[NUM_PINS] = {0};

static kos_msg_t handle_request(kos_msg_t msg, seL4_Word badge, seL4_Word caller_id) {
  if (badge != PWM_PROTOCOL_BADGE)
    return kos_msg_new_status(STATUS_NOT_IMPLEMENTED);
  if (caller_id == 0)
    return kos_msg_new_status(STATUS_BAD_REQUEST);
  if (client_id != 0 && client_id != caller_id)
    // Don't support more than one client
    return kos_msg_new_status(STATUS_FULL);

  // Create the token that we send to the client
  kos_assert_created(
    kos_msg_token_create(
      PWM_PROTOCOL_BADGE, // IN seL4_Word badge,
      KOS_MSG_FLAG_SEND_PAYLOAD, // IN uint8_t flags,
      TRANSFER_TOKEN_SLOT
    ),
    "create pwm token to give"
  );

  client_id = caller_id;

  return kos_msg_new(STATUS_OK, 0, 0, TRANSFER_TOKEN_SLOT, 0);
}

static void calc_and_set_counter_values(int pin, uint32_t duty_cycle) {
  if (curr_freq == 0) {
    // Avoid divide by zero
    return;
  }

  unsigned int controller_base = (unsigned int) pwm_controller_base;

  uint32_t period_count = TB_CLK / curr_freq;
  uint32_t counter_value = (uint32_t) (duty_cycle / 100.0 * period_count);

  if (pin == PIN_A) {
    EHRPWMLoadCMPA(controller_base,
                   counter_value,
                   EHRPWM_SHADOW_WRITE_ENABLE,
                   EHRPWM_CMPCTL_LOADAMODE_TBCTRZERO,
                   EHRPWM_CMPCTL_OVERWR_SH_FL);
  } else {
    EHRPWMLoadCMPB(controller_base,
                   counter_value,
                   EHRPWM_SHADOW_WRITE_ENABLE,
                   EHRPWM_CMPCTL_LOADAMODE_TBCTRZERO,
                   EHRPWM_CMPCTL_OVERWR_SH_FL);
  }
}

static kos_msg_t handle_set_pwm_frequency(kos_msg_t msg, seL4_Word caller_id) {
  if (caller_id != client_id)
    return kos_msg_new_status(STATUS_UNAUTHORIZED);
  if (kos_msg_payload_size(msg.metadata) != sizeof(uint32_t) * SET_PWM_FREQUENCY_ARGS)
    return kos_msg_new_status(STATUS_BAD_REQUEST);

  uint32_t *transport = kos_msg_server_payload();

  uint32_t frequency = transport[0];

  unsigned int controller_base = (unsigned int) pwm_controller_base;

  // Set the frequency
  EHRPWMPWMOpFreqSet(controller_base, TB_CLK, frequency, EHRPWM_COUNT_UP, EHRPWM_SHADOW_WRITE_ENABLE);

  curr_freq = frequency;

  // Update the counter values of the PWMs
  calc_and_set_counter_values(PIN_A, curr_pin_duty_cycle[PIN_A]);
  calc_and_set_counter_values(PIN_B, curr_pin_duty_cycle[PIN_B]);

  return kos_msg_new_status(STATUS_OK);
}

static kos_msg_t handle_set_pwm_duty_cycle(kos_msg_t msg, seL4_Word caller_id) {
  if (caller_id != client_id)
    return kos_msg_new_status(STATUS_UNAUTHORIZED);
  if (kos_msg_payload_size(msg.metadata) != sizeof(uint32_t) * SET_PWM_DUTY_CYCLE_ARGS)
    return kos_msg_new_status(STATUS_BAD_REQUEST);

  uint32_t *transport = kos_msg_server_payload();

  uint32_t pin = transport[0];
  uint32_t duty_cycle = transport[1];

  unsigned int controller_base = (unsigned int) pwm_controller_base;

  // Load the duty cycle value in
  if (pin == PIN_A) {
    curr_pin_duty_cycle[PIN_A] = duty_cycle;
    calc_and_set_counter_values(PIN_A, curr_pin_duty_cycle[PIN_A]);
  } else {
    curr_pin_duty_cycle[PIN_B] = duty_cycle;
    calc_and_set_counter_values(PIN_B, curr_pin_duty_cycle[PIN_B]);
  }

  return kos_msg_new_status(STATUS_OK);
}

static void listen_thread_fn(IN kos_thread_environment_t* p_env, IN seL4_Word garbage) {
  // Publish the KOS am335x PWM protocol
  kos_assert_ok(
    kos_dir_publish_str(
      protocol_name,
      PWM_REQUEST_LABEL,
      PWM_PROTOCOL_BADGE,
      KOS_MSG_FLAG_SEND_PAYLOAD
    ),
    "failed to publish AM335X PWM protocol"
  );

  // Signal that we are done initializing
  kos_app_ready();

  // Prepare to receive caps
  kos_cap_t receive_cap = kos_cnode_cap(p_env->p_cnode, KOS_THREAD_SLOT_RECEIVE);
  kos_cap_set_receive(receive_cap);

  // Prepare the reply cap
  kos_cap_t reply_cap = kos_cnode_cap(p_env->p_cnode, KOS_THREAD_SLOT_REPLY);

  // A slot to hold the transport
  kos_cap_t server_cap = kos_cap_reserve();

  // Set up the server transport
  kos_assert_created(
    kos_msg_server_create(
      server_cap,
      reply_cap, // IN kos_cap_t reply_cap,
      RECEIVE_TOKEN_SLOT, // IN kos_token_t receive_token_slot,
      &server // OUT kos_msg_server_t* p_server
    ),
    NULL
  );

  // No longer need to receive caps.
  kos_cap_clear_receive();

  // Initial status is OK
  kos_msg_t msg = kos_msg_new_status(STATUS_OK);

  while (true) {
    seL4_Word badge;
    seL4_Word seL4_badge;
    seL4_Word caller_id;

    seL4_SetMR(0, msg.label);
    seL4_SetMR(1, msg.param);
    seL4_SetMR(2, msg.metadata);
    seL4_MessageInfo_t sel4_msg = seL4_ReplyRecv(
      server.transport.ep_cptr,
      seL4_MessageInfo_new(STATUS_OK, 0, 0, 3),
      &seL4_badge,
      server.reply_cptr);

    // fill out the message struct
    msg.label = seL4_GetMR(0);
    msg.param = seL4_GetMR(1);
    msg.metadata = seL4_GetMR(2);
    badge = seL4_GetMR(3);

    caller_id = seL4_MessageInfo_get_label(sel4_msg);

    // Check the protocol via the badge first
    if (badge != PWM_PROTOCOL_BADGE) {
      msg = kos_msg_new(STATUS_NOT_IMPLEMENTED, 0, 0, 0, 0);
      continue;
    }

    // Act on the label
    switch (msg.label) {
      case PWM_REQUEST_LABEL:
        msg = handle_request(msg, badge, caller_id);
        break;
      case SET_PWM_FREQUENCY_REQUEST:
        msg = handle_set_pwm_frequency(msg, caller_id);
        break;
      case SET_PWM_DUTY_CYCLE_REQUEST:
        msg = handle_set_pwm_duty_cycle(msg, caller_id);
        break;
      default:
        msg = kos_msg_new(STATUS_NOT_IMPLEMENTED, 0, 0, 0, 0);
        break;
    }
  }
}

static void init_pwm_controller() {
  // Note that the pwmss timebase clocks have to be configured in the control
  // module (offset 0x664, bits 0 to 2), this requires privileged mode to write
  // to that register

  unsigned int controller_base = (unsigned int) pwm_controller_base;

  // Disable the clock
  EHRPWMClockDisable(controller_base);

  // Configure the clock frequency
  EHRPWMTimebaseClkConfig(controller_base, TB_CLK, MODULE_CLK);

  // Disable stuff that we don't need:
  // - sychnronisation
  // - sync out
  // - dead-band
  // - trip events
  // - PWM chopping
  // - High resolution PWM
  EHRPWMTimebaseSyncDisable(controller_base);
  EHRPWMSyncOutModeSet(controller_base, EHRPWM_SYNCOUT_DISABLE);
  EHRPWMDBOutput(controller_base, EHRPWM_DBCTL_OUT_MODE_BYPASS);
  EHRPWMTZTripEventDisable(controller_base, EHRPWM_TZ_ONESHOT);
  EHRPWMTZTripEventDisable(controller_base, EHRPWM_TZ_CYCLEBYCYCLE);
  EHRPWMChopperDisable(controller_base);
  EHRPWMHRDisable(controller_base);

  // Configure the emulation behaviour
  EHRPWMTBEmulationModeSet(controller_base, EHRPWM_STOP_AFTER_NEXT_TB_INCREMENT);

  // Clear any interrupts and then disable them
  EHRPWMETIntClear(controller_base);
  EHRPWMETIntDisable(controller_base);

  // Configure the action qualifiers for the two PWMs to output a high-to-low
  // signal
  EHRPWMConfigureAQActionOnA(controller_base,
                             EHRPWM_AQCTLA_ZRO_EPWMXAHIGH,
                             EHRPWM_AQCTLA_PRD_DONOTHING,
                             EHRPWM_AQCTLA_CAU_EPWMXALOW,
                             EHRPWM_AQCTLA_CAD_DONOTHING,
                             EHRPWM_AQCTLA_CBU_DONOTHING,
                             EHRPWM_AQCTLA_CBD_DONOTHING,
                             EHRPWM_AQSFRC_ACTSFA_DONOTHING);

  EHRPWMConfigureAQActionOnB(controller_base,
                             EHRPWM_AQCTLB_ZRO_EPWMXBHIGH,
                             EHRPWM_AQCTLB_PRD_DONOTHING,
                             EHRPWM_AQCTLB_CAU_DONOTHING,
                             EHRPWM_AQCTLB_CAD_DONOTHING,
                             EHRPWM_AQCTLB_CBU_EPWMXBLOW,
                             EHRPWM_AQCTLB_CBD_DONOTHING,
                             EHRPWM_AQSFRC_ACTSFB_DONOTHING);

  // Load zero into the counters for the two PWMs, this will set the duty cycle
  // to 0 and output a low signal and effectively 'stop' it
  EHRPWMLoadCMPA(controller_base,
                 0,
                 EHRPWM_SHADOW_WRITE_ENABLE,
                 EHRPWM_CMPCTL_LOADAMODE_TBCTRZERO,
                 EHRPWM_CMPCTL_OVERWR_SH_FL);

  EHRPWMLoadCMPB(controller_base,
                 0,
                 EHRPWM_SHADOW_WRITE_ENABLE,
                 EHRPWM_CMPCTL_LOADAMODE_TBCTRZERO,
                 EHRPWM_CMPCTL_OVERWR_SH_FL);

  // Enable the clock
  EHRPWMClockEnable(controller_base);
}

int main(int argc, char *argv[]) {
#ifdef VISUALIZE_STARTUP
  kos_printf("\n");
  kos_printf("----  %s server ----\n", argv[0]);
#endif

  kos_assert_eq(argc, EXPECTED_ARGC, "unexpected argument counts");

  protocol_name = argv[PROTOCOL_NAME_IDX];

  // Initialize the thread manager
  kos_assert_ok(
    kos_thread_mgr_init(
      &root_manager_entry, // IN_OUT kos_thread_mgr_entry_t* p_entries,
      1, // IN seL4_Word capacity,
      &root_thread_mgr // OUT kos_thread_mgr_t* p_thread_mgr
    ),
    NULL
  );

  // Bootstrap the message server connection
  kos_assert_created(kos_msg_setup(), NULL);

  // Find the frame of the PWM controller, it could be any one of the
  // controllers that were given to us
  kos_status_t status = STATUS_NOT_FOUND;
  for (int i = 0; i < NUM_PWM; i++) {
    kos_cap_t dummy_cap;
    status = kos_dev_resources_find_device_frame(&pwm_controller_frames[i], &dummy_cap);
    if (status == STATUS_OK) {
      status = kos_dev_resources_map_device_frame(&pwm_controller_frames[i],
                                                  kos_cap_rights_all_rights(),
                                                  NULL, &pwm_controller_base);
      // The PWM register set is 0x200 off the base, there are other submodules before this
      pwm_controller_base += 0x200;

      break;
    }
  }
  kos_assert_ok(status, "failed to map a PWM controller");

  // Initialize the PWM controller now
  init_pwm_controller();

  // Create and start the listener thread
  kos_assert_created(
    kos_thread_create(listen_thread_fn, 0, false, &listener_thread),
    "failed to create listener thread"
  );

  kos_assert_ok(
    kos_thread_mgr_add(
      &root_thread_mgr, // IN_OUT kos_thread_mgr_t* p_thread_mgr,
      &listener_thread, // IN_OUT kos_thread_t* p_thread,
      KOS_THREAD_MGR_NO_LIMIT, // IN seL4_Word fault_limit,
      0, // IN seL4_Word cookie,
      kos_thread_fault_fn_print_faults, // OPTIONAL IN kos_thread_fault_fn fault_fn
      NULL // OPTIONAL OUT seL4_Word* p_id
    ),
    "failed to add listener thread to the thread manager"
  );

  kos_assert_ok(
    kos_thread_start(&listener_thread), // IN_OUT kos_thread_t* p_thread,
    "failed to start listener thread"
  );

  // Run app-level thread manager handler directly on this thread,
  // this should never return
  kos_thread_mgr_direct_handler(&root_thread_mgr);
  kos_stop("KOS am335x PWM server exited unexpectedly");
  return 0;
}
