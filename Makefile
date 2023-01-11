# Copyright (c) 2023, Kry10 Limited. All rights reserved.
#
# SPDX-License-Identifier: LicenseRef-Kry10

INCLUDES=c_src/gpio_v2.h c_src/hw_gpio_v2.h c_src/hw_types.h
SRCS=c_src/kos_am335x_starterware_gpio.c c_src/gpio_v2.c

# Create kos_am335x_starterware_pwm.so in the priv/ folder of the application.
$(MIX_COMPILE_PATH)/../priv/kos_am335x_starterware_gpio.so: $(SRCS) $(INCLUDES)
	mkdir -p $(MIX_COMPILE_PATH)/../priv/
	$(CC) $(KOS_RUNTIME_CFLAGS) -fPIC -I$(ERL_EI_INCLUDE_DIR) $(SRCS) -shared -o $@
