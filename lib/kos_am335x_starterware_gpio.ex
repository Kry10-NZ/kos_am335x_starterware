# Copyright (c) 2023, Kry10 Limited. All rights reserved.
#
# SPDX-License-Identifier: LicenseRef-Kry10

defmodule KosAm335xStarterwareGpio.Nif do
  @compile {:autoload, false}
  @on_load :load_nifs

  @doc false
  def load_nifs do
    :ok = case :os.type() do
      {:unix, :kos} ->
        :kos_am335x_starterware_gpio
          |> :code.priv_dir()
          |> :filename.join('kos_am335x_starterware_gpio')
          |> :erlang.load_nif(0)
      _ ->
        :ok
    end
  end

  def configure_pin(_pin, _direction), do: :erlang.nif_error("Did not find GPIO controllers")
  def read(_pin), do: :erlang.nif_error("Did not find GPIO controllers")
  def write(_pin, _level), do: :erlang.nif_error("Did not find GPIO controllers")
  def set_debounce(_pin, _debounce), do: :erlang.nif_error("Did not find GPIO controllers")
  def set_debounce_time(_pin, _time), do: :erlang.nif_error("Did not find GPIO controllers")
end
