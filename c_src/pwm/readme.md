<!--
Copyright (c) 2023, Kry10 Limited. All rights reserved.

SPDX-License-Identifier: LicenseRef-Kry10
-->

# AM335X PWMSS Driver

This driver is based on the driver included in Texas Instruments' StarterWare
package [1]. Files beside from the `kos_am335x_pwm.c` are taken directly from
the StarterWare package and have little to no modifications.

In particular, in the `ti` folder:
- The `*.c` files are taken from the `driver` folder in the package.
- The `*.h` files are taken from the `include` folder in the package.
  + The `hw_*.h` files are taken from the `include/hw` folder in the
    package.

The `kos_am335x_pwm.c` file in this folder contains definitions related to a KOS
application shim that serves requests to access the PWM controller.

[1]: https://github.com/embest-tech/AM335X_StarterWare_02_00_01_01
