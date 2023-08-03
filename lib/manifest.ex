# Copyright (c) 2023, Kry10 Limited. All rights reserved.
#
# SPDX-License-Identifier: LicenseRef-Kry10

defmodule KosAm335xStarterware.Manifest do
  alias KosManifest.{Context, App}

  @gpio_protocol "am335x_gpio_protocol"
  @pwm_protocol "am335x_pwm_protocol"

  @gpio_clock_setups [
    %KosClock.Setup{offset: 0xac, value_to_set: 0x40002, expected_result: 0x40002},
    %KosClock.Setup{offset: 0xb0, value_to_set: 0x40002, expected_result: 0x40002},
    %KosClock.Setup{offset: 0xb4, value_to_set: 0x40002, expected_result: 0x40002},
    %KosClock.Setup{offset: 0x408, value_to_set: 0x40002, expected_result: 0x40002}
  ]

  @pwm_resources [
    [%{ address: 0x48300000, size: 0x1000 }],
    [%{ address: 0x48302000, size: 0x1000 }],
    [%{ address: 0x48304000, size: 0x1000 }],
  ]

  @pwm_clock_setups [
    [%KosClock.Setup{offset: 0xd4, value_to_set: 2, expected_result: 2}],
    [%KosClock.Setup{offset: 0xcc, value_to_set: 2, expected_result: 2}],
    [%KosClock.Setup{offset: 0xd8, value_to_set: 2, expected_result: 2}],
  ]

  @pwm_ids [0,1,2]

  @spec include_gpio(Context.t(), Keyword.t()) :: {:ok, Context.t(), App.t(), String.t()} | {:error, any}
  def include_gpio(context, opts \\ []) do
    protocol = Keyword.get(opts, :protocol, @gpio_protocol)
    msg_server = Keyword.get(opts, :msg_server, Context.default_msg_server())

    gpio = gpio_definition(protocol)
    {:ok, context} =
      :kos_am335x_starterware
      |> Application.app_dir()
      |> Path.join("cmake/kos_am335x_gpio")
      |> then(&Context.put_binary(context, "kos_am335x_gpio", &1))

    add_and_publish(context, gpio, protocol, @gpio_clock_setups, msg_server)
  end

  @spec include_pwm(Context.t(), Keyword.t()) :: {:ok, Context.t(), App.t(), String.t()} | {:error, any}
  def include_pwm(context, opts \\ []) do
    protocol = Keyword.get(opts, :protocol, @pwm_protocol)
    msg_server = Keyword.get(opts, :msg_server, Context.default_msg_server())
    pwm_id = Keyword.get(opts, :am335x_pwm_id, 0)
    {:ok, context} =
      :kos_am335x_starterware
      |> Application.app_dir()
      |> Path.join("cmake/kos_am335x_pwm")
      |> then(&Context.put_binary(context, "kos_am335x_pwm", &1))

    if pwm_id not in @pwm_ids do
      {:error, :invalid_pwm_id}
    else
      pwm = pwm_definition(protocol, pwm_id)

      clock_setup = pwm_clock_setup(pwm_id)

      add_and_publish(context, pwm, protocol, clock_setup, msg_server)
    end
  end

  defp add_and_publish(context, app, protocol, clock_setups, msg_server) do
    with {:ok, context, app} <- Context.put_app(context, app),
         {:ok, context} <- Context.register_clock_setup(context, app, clock_setups),
         {:ok, context} <- Context.publish_protocol(context, app, protocol, msg_server) do
      {:ok, context, app, protocol}
    else
      {:error, reason} -> {:error, reason}
    end
  end

  defp gpio_definition(protocol) do
    %{
      name: "am335x_gpio",
      binary: "kos_am335x_gpio",
      heap_pages: 16,
      ut_large_pages: 16,
      ut_4k_pages: 32,
      max_priority: 150,
      priority: 145,
      arguments: [protocol],
      resources: %{
        device_frames: [
          %{ address: 0x44e07000, size: 0x1000 },
          %{ address: 0x4804c000, size: 0x1000 },
          %{ address: 0x481ac000, size: 0x1000 },
          %{ address: 0x481ae000, size: 0x1000 }
        ],
      }
    }
  end

  defp pwm_definition(protocol, pwm_id) do
    pwm_resource = Enum.at(@pwm_resources, pwm_id)
    %{
      name: "am335x_pwm",
      binary: "kos_am335x_pwm",
      heap_pages: 16,
      ut_large_pages: 16,
      ut_4k_pages: 32,
      max_priority: 150,
      priority: 145,
      arguments: [protocol],
      resources: %{
        device_frames: pwm_resource
      }
    }
  end

  defp pwm_clock_setup(pwm_id) do
    Enum.at(@pwm_clock_setups, pwm_id)
  end
end
