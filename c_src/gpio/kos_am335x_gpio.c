// Copyright (c) 2023, Kry10 Limited. All rights reserved.
//
// SPDX-License-Identifier: LicenseRef-Kry10

#include <kos.h>

#include "gpio_v2.h"

#define VISUALIZE_STARTUP

#define EXPECTED_ARGC 2
#define PROTOCOL_NAME_IDX 1

#define GPIO_REQUEST_LABEL (~0)
#define GPIO_PROTOCOL_BADGE (KOS_CORE_APP_ID_LIMIT)

#define RECEIVE_TOKEN_SLOT 1
#define TRANSFER_TOKEN_SLOT 2

#define AM335X_GPIO0_PADDR 0x44e07000
#define AM335X_GPIO1_PADDR 0x4804c000
#define AM335X_GPIO2_PADDR 0x481ac000
#define AM335X_GPIO3_PADDR 0x481ae000
#define NUM_GPIOS 4

#define CONFIGURE_PIN_ARGS 2
#define SET_DEBOUNCE_ARGS 2
#define SET_DEBOUNCE_TIMING_ARGS 2
#define READ_ARGS 1
#define WRITE_ARGS 2

#define INPUT_MODE 0
#define OUTPUT_MODE 1

#define PINS_IN_CONTROLLER 32
#define MAX_PIN 127

enum request_label {
  CONFIGURE_PIN_REQUEST = 1,
  SET_DEBOUNCE_REQUEST,
  SET_DEBOUNCE_TIMING_REQUEST,
  READ_REQUEST,
  WRITE_REQUEST,
  NUM_GPIO_REQUESTS
};

static kos_device_frame_t gpio_controller_frames[] = {
  {.paddr = AM335X_GPIO0_PADDR, .size = KOS_EXP2(seL4_PageBits)},
  {.paddr = AM335X_GPIO1_PADDR, .size = KOS_EXP2(seL4_PageBits)},
  {.paddr = AM335X_GPIO2_PADDR, .size = KOS_EXP2(seL4_PageBits)},
  {.paddr = AM335X_GPIO3_PADDR, .size = KOS_EXP2(seL4_PageBits)}
};

static seL4_Word gpio_controller_bases[4];

static kos_msg_server_t server;
static kos_thread_mgr_entry_t root_manager_entry;
static kos_thread_mgr_t root_thread_mgr;
static kos_thread_t listener_thread;

static char* protocol_name;

// ID of the only client that can be registered with us
// TODO: Multi-client support
seL4_Word client_id;

// Pin's given to us by the Elixir front-end are a flat number from 0 to 127.
// Each controller (there are four) controls 32 pins.
static inline unsigned int flat_pin_to_controller_pin(unsigned int flat_pin, unsigned int *controller, unsigned int *pin) {
  *controller = flat_pin / PINS_IN_CONTROLLER;
  *pin = flat_pin % PINS_IN_CONTROLLER;
}

static kos_msg_t handle_request(kos_msg_t msg, seL4_Word badge, seL4_Word caller_id) {
  if (badge != GPIO_PROTOCOL_BADGE)
    return kos_msg_new_status(STATUS_NOT_IMPLEMENTED);
  if (caller_id == 0)
    return kos_msg_new_status(STATUS_BAD_REQUEST);
  if (client_id != 0 && client_id != caller_id)
    // Don't support more than one client
    return kos_msg_new_status(STATUS_FULL);

  // Create the token that we send to the client
  kos_assert_created(
    kos_msg_token_create(
      GPIO_PROTOCOL_BADGE, // IN seL4_Word badge,
      KOS_MSG_FLAG_SEND_PAYLOAD, // IN uint8_t flags,
      TRANSFER_TOKEN_SLOT
    ),
    "create gpio token to give"
  );

  client_id = caller_id;

  return kos_msg_new(STATUS_OK, 0, 0, TRANSFER_TOKEN_SLOT, 0);
}

static kos_msg_t handle_configure_pin(kos_msg_t msg, seL4_Word caller_id) {
  if (caller_id != client_id)
    return kos_msg_new_status(STATUS_UNAUTHORIZED);
  if (kos_msg_payload_size(msg.metadata) != sizeof(uint32_t) * CONFIGURE_PIN_ARGS)
    return kos_msg_new_status(STATUS_BAD_REQUEST);

  uint32_t *transport = kos_msg_server_payload();

  uint32_t pin = transport[0];
  uint32_t mode = transport[1];

  if (pin > MAX_PIN) {
    return kos_msg_new_status(STATUS_BAD_REQUEST);
  }

  unsigned int controller = 0;

  flat_pin_to_controller_pin(pin, &controller, &pin);

  unsigned int controller_base = (unsigned int) gpio_controller_bases[controller];

  if (mode == OUTPUT_MODE) {
    GPIODirModeSet(controller_base, pin, GPIO_DIR_OUTPUT);
  } else {
    GPIODirModeSet(controller_base, pin, GPIO_DIR_INPUT);
  }

  return kos_msg_new_status(STATUS_OK);
}

static kos_msg_t handle_set_debounce(kos_msg_t msg, seL4_Word caller_id) {
  if (caller_id != client_id)
    return kos_msg_new_status(STATUS_UNAUTHORIZED);
  if (kos_msg_payload_size(msg.metadata) != sizeof(uint32_t) * SET_DEBOUNCE_ARGS)
    return kos_msg_new_status(STATUS_BAD_REQUEST);

  uint32_t *transport = kos_msg_server_payload();

  uint32_t pin = transport[0];
  uint32_t debounce = transport[1];

  if (pin > MAX_PIN) {
    return kos_msg_new_status(STATUS_BAD_REQUEST);
  }

  unsigned int controller = 0;

  flat_pin_to_controller_pin(pin, &controller, &pin);

  unsigned int controller_base = (unsigned int) gpio_controller_bases[controller];

  if (debounce) {
    GPIODebounceFuncControl(controller_base, pin, GPIO_DEBOUNCE_FUNC_ENABLE);
  } else {
    GPIODebounceFuncControl(controller_base, pin, GPIO_DEBOUNCE_FUNC_DISABLE);
  }

  return kos_msg_new_status(STATUS_OK);
}

static kos_msg_t handle_set_debounce_timing(kos_msg_t msg, seL4_Word caller_id) {
  if (caller_id != client_id)
    return kos_msg_new_status(STATUS_UNAUTHORIZED);
  if (kos_msg_payload_size(msg.metadata) != sizeof(uint32_t) * SET_DEBOUNCE_TIMING_ARGS)
    return kos_msg_new_status(STATUS_BAD_REQUEST);

  uint32_t *transport = kos_msg_server_payload();

  uint32_t pin = transport[0];
  uint32_t debounce_time = transport[1];

  if (pin > MAX_PIN) {
    return kos_msg_new_status(STATUS_BAD_REQUEST);
  }

  unsigned int controller = 0;

  flat_pin_to_controller_pin(pin, &controller, &pin);

  unsigned int controller_base = (unsigned int) gpio_controller_bases[controller];

  GPIODebounceTimeConfig(controller_base, debounce_time);

  return kos_msg_new_status(STATUS_OK);
}

static kos_msg_t handle_read(kos_msg_t msg, seL4_Word caller_id) {
  if (caller_id != client_id)
    return kos_msg_new_status(STATUS_UNAUTHORIZED);
  if (kos_msg_payload_size(msg.metadata) != sizeof(uint32_t) * READ_ARGS)
    return kos_msg_new_status(STATUS_BAD_REQUEST);

  uint32_t *transport = kos_msg_server_payload();

  uint32_t pin = transport[0];

  if (pin > MAX_PIN) {
    return kos_msg_new_status(STATUS_BAD_REQUEST);
  }

  unsigned int controller = 0;

  flat_pin_to_controller_pin(pin, &controller, &pin);

  unsigned int controller_base = (unsigned int) gpio_controller_bases[controller];

  uint32_t level = !!(GPIOPinRead(controller_base, pin));

  transport[0] = level;

  return kos_msg_new(STATUS_OK, 0, sizeof(uint32_t), 0, 0);
}

static kos_msg_t handle_write(kos_msg_t msg, seL4_Word caller_id) {
  if (caller_id != client_id)
    return kos_msg_new_status(STATUS_UNAUTHORIZED);
  if (kos_msg_payload_size(msg.metadata) != sizeof(uint32_t) * WRITE_ARGS)
    return kos_msg_new_status(STATUS_BAD_REQUEST);

  uint32_t *transport = kos_msg_server_payload();

  uint32_t pin = transport[0];
  uint32_t level = transport[1];

  if (pin > MAX_PIN) {
    return kos_msg_new_status(STATUS_BAD_REQUEST);
  }

  unsigned int controller = 0;

  flat_pin_to_controller_pin(pin, &controller, &pin);

  unsigned int controller_base = (unsigned int) gpio_controller_bases[controller];

  GPIOPinWrite(controller_base, pin, level);

  return kos_msg_new_status(STATUS_OK);
}

static void listen_thread_fn(IN kos_thread_environment_t* p_env, IN seL4_Word garbage) {
  // Publish the KOS am335x GPIO protocol
  kos_assert_ok(
    kos_dir_publish_str(
      protocol_name,
      GPIO_REQUEST_LABEL,
      GPIO_PROTOCOL_BADGE,
      KOS_MSG_FLAG_SEND_PAYLOAD
    ),
    "failed to publish AM335X GPIO protocol"
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
    if (badge != GPIO_PROTOCOL_BADGE) {
      msg = kos_msg_new(STATUS_NOT_IMPLEMENTED, 0, 0, 0, 0);
      continue;
    }

    // Act on the label
    switch (msg.label) {
      case GPIO_REQUEST_LABEL:
        msg = handle_request(msg, badge, caller_id);
        break;
      case CONFIGURE_PIN_REQUEST:
        msg = handle_configure_pin(msg, caller_id);
        break;
      case SET_DEBOUNCE_REQUEST:
        msg = handle_set_debounce(msg, caller_id);
        break;
      case SET_DEBOUNCE_TIMING_REQUEST:
        msg = handle_set_debounce_timing(msg, caller_id);
        break;
      case READ_REQUEST:
        msg = handle_read(msg, caller_id);
        break;
      case WRITE_REQUEST:
        msg = handle_write(msg, caller_id);
        break;
      default:
        msg = kos_msg_new(STATUS_NOT_IMPLEMENTED, 0, 0, 0, 0);
        break;
    }
  }
}

static void init_gpio_modules(seL4_Word base_addr) {
  unsigned int controller_base = (unsigned int) base_addr;

  GPIOModuleDisable(controller_base);

  GPIOModuleReset(controller_base);

  GPIOAutoIdleModeControl(controller_base, GPIO_AUTO_IDLE_MODE_DISABLE);

  GPIOIdleModeConfigure(controller_base, GPIO_IDLE_MODE_NO_IDLE);

  GPIOModuleEnable(controller_base);
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

  // Map the frames of the GPIO controllers
  for (int i = 0; i < NUM_GPIOS; i++) {
    kos_status_t status = kos_dev_resources_map_device_frame(&gpio_controller_frames[i],
                                                             kos_cap_rights_all_rights(),
                                                             NULL,
                                                             &gpio_controller_bases[i]);
    kos_assert_ok(status, "failed to map GPIO controller %d", i);
  }

  // Initialise the modules
  for (int i = 0; i < NUM_GPIOS; i++) {
    init_gpio_modules(gpio_controller_bases[i]);
  }

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
  kos_stop("KOS am335x GPIO server exited unexpectedly");
  return 0;
}
