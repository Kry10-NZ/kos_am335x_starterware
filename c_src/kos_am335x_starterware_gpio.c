// Copyright (c) 2023, Kry10 Limited. All rights reserved.
//
// SPDX-License-Identifier: LicenseRef-Kry10

#include <erl_nif.h>

#include <kos_utils.h>
#include <kos.h>
#include <kos_utils/memory/mapping.h>

#include "gpio_v2.h"

#define AM335X_GPIO0_PADDR 0x44e07000
#define AM335X_GPIO1_PADDR 0x4804C000
#define AM335X_GPIO2_PADDR 0x481ac000
#define AM335X_GPIO3_PADDR 0x481ae000
#define NUM_GPIOS 4

#define CONFIGURE_PIN_ARGS 2
#define READ_ARGS 1
#define WRITE_ARGS 2
#define SET_DEBOUNCE_ARGS 2
#define SET_DEBOUNCE_TIME_ARGS 2

#define INPUT_MODE 0
#define OUTPUT_MODE 1

#define PINS_IN_CONTROLLER 32

static kos_device_frame_t gpio_controller_frames[] = {
  {.paddr = AM335X_GPIO0_PADDR, .size = KOS_EXP2(seL4_PageBits)},
  {.paddr = AM335X_GPIO1_PADDR, .size = KOS_EXP2(seL4_PageBits)},
  {.paddr = AM335X_GPIO2_PADDR, .size = KOS_EXP2(seL4_PageBits)},
  {.paddr = AM335X_GPIO3_PADDR, .size = KOS_EXP2(seL4_PageBits)}
};

static seL4_Word gpio_controller_bases[4];

// Pin's given to us by the Elixir front-end are a flat number from 0 to 127.
// Each controller (there are four) controls 32 pins.
static inline unsigned int flat_pin_to_controller_pin(unsigned int flat_pin, unsigned int *controller, unsigned int *pin) {
  *controller = flat_pin / PINS_IN_CONTROLLER;
  *pin = flat_pin % PINS_IN_CONTROLLER;
}

static ERL_NIF_TERM configure_pin(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  unsigned int pin = 0;
  unsigned int mode = 0;

  if (argc != CONFIGURE_PIN_ARGS
      || !enif_get_uint(env, argv[0], &pin)
      || !enif_get_uint(env, argv[1], &mode)) {
    return enif_make_badarg(env);
  }

  unsigned int controller = 0;

  flat_pin_to_controller_pin(pin, &controller, &pin);

  unsigned int controller_base = (unsigned int) gpio_controller_bases[controller];

  if (mode == OUTPUT_MODE) {
    GPIODirModeSet(controller_base, pin, GPIO_DIR_OUTPUT);
  } else {
    GPIODirModeSet(controller_base, pin, GPIO_DIR_INPUT);
  }

  return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM set_debounce(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  unsigned int pin = 0;
  unsigned int debounce = 0;

  if (argc != CONFIGURE_PIN_ARGS
      || !enif_get_uint(env, argv[0], &pin)
      || !enif_get_uint(env, argv[1], &debounce)) {
    return enif_make_badarg(env);
  }

  unsigned int controller = 0;

  flat_pin_to_controller_pin(pin, &controller, &pin);

  unsigned int controller_base = (unsigned int) gpio_controller_bases[controller];

  if (debounce) {
    GPIODebounceFuncControl(controller_base, pin, GPIO_DEBOUNCE_FUNC_ENABLE);
  } else {
    GPIODebounceFuncControl(controller_base, pin, GPIO_DEBOUNCE_FUNC_DISABLE);
  }

  return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM set_debounce_time(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  unsigned int pin = 0;
  unsigned int debounce_time = 0;

  if (argc != CONFIGURE_PIN_ARGS
      || !enif_get_uint(env, argv[0], &pin)
      || !enif_get_uint(env, argv[1], &debounce_time)) {
    return enif_make_badarg(env);
  }

  unsigned int controller = 0;

  flat_pin_to_controller_pin(pin, &controller, &pin);

  unsigned int controller_base = (unsigned int) gpio_controller_bases[controller];

  GPIODebounceTimeConfig(controller_base, debounce_time);

  return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM read(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  unsigned int pin = 0;

  if (argc != CONFIGURE_PIN_ARGS
      || !enif_get_uint(env, argv[0], &pin)) {
    return enif_make_badarg(env);
  }

  unsigned int controller = 0;

  flat_pin_to_controller_pin(pin, &controller, &pin);

  unsigned int controller_base = (unsigned int) gpio_controller_bases[controller];

  unsigned int level = !!(GPIOPinRead(controller_base, pin));

  return enif_make_tuple2(env,
                          enif_make_atom(env, "ok"),
                          enif_make_uint(env, level));
}

static ERL_NIF_TERM write(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  unsigned int pin = 0;
  unsigned int level = 0;

  if (argc != CONFIGURE_PIN_ARGS
      || !enif_get_uint(env, argv[0], &pin)
      || !enif_get_uint(env, argv[1], &level)) {
    return enif_make_badarg(env);
  }

  unsigned int controller = 0;

  flat_pin_to_controller_pin(pin, &controller, &pin);

  unsigned int controller_base = (unsigned int) gpio_controller_bases[controller];

  GPIOPinWrite(controller_base, pin, level);

  return enif_make_atom(env, "ok");
}

static void init_gpio_modules(seL4_Word base_addr) {
  unsigned int controller_base = (unsigned int) base_addr;

  GPIOModuleDisable(controller_base);

  GPIOModuleReset(controller_base);

  GPIOAutoIdleModeControl(controller_base, GPIO_AUTO_IDLE_MODE_DISABLE);

  GPIOIdleModeConfigure(controller_base, GPIO_IDLE_MODE_NO_IDLE);

  GPIOModuleEnable(controller_base);
}

static int load(ErlNifEnv* env, void** priv_data, ERL_NIF_TERM load_info) {
  for (int i = 0; i < NUM_GPIOS; i++) {
    kos_status_t status = kos_dev_resources_map_device_frame(&gpio_controller_frames[i],
                                                             kos_cap_rights_all_rights(),
                                                             NULL,
                                                             &gpio_controller_bases[i]);
    kos_assert_ok(status, "failed to map a GPIO controller %d", i);
  }

  // Initialise the modules
  for (int i = 0; i < NUM_GPIOS; i++) {
    init_gpio_modules(gpio_controller_bases[i]);
  }

  return 0;
}

static ErlNifFunc nif_funcs[] = {
  // {erl_function_name, erl_function_arity, c_function}
  {"configure_pin", CONFIGURE_PIN_ARGS, configure_pin, 0},
  {"read", READ_ARGS, read, 0},
  {"write", WRITE_ARGS, write, 0},
  {"set_debounce", SET_DEBOUNCE_ARGS, set_debounce, 0},
  {"set_debounce_time", SET_DEBOUNCE_TIME_ARGS, set_debounce_time, 0},
};

ERL_NIF_INIT(Elixir.KosAm335xStarterWareGpio, nif_funcs, load, NULL, NULL, NULL)
