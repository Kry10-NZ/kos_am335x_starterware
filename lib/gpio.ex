# Copyright (c) 2023, Kry10 Limited. All rights reserved.
#
# SPDX-License-Identifier: LicenseRef-Kry10

defmodule KosAm335xStarterwareGpio do
  @moduledoc """
  Control AM335X GPIO pins in Elixir.

  This uses a NIF that contains the Texas Instruments Starterware GPIO driver to
  control the GPIO pins.
  """

  alias KosAm335xStarterwareGpio.Nif

  @typedoc "Directions allowed for the GPIO pins"
  @type direction :: :input | :output

  @typedoc "Input/output signal levels"
  @type level :: :low | :high

  @max_pin 127
  @max_debounce_time 255

  @input_direction 0
  @output_direction 1

  @low_level 0
  @high_level 1

  @doc """
  Configures the direction of a `pin`.

  `pin` should be a value between 0 and 127, inclusive. `direction` can be of
  either `:input` or `:output`.
  """
  @spec configure_pin(non_neg_integer(), direction()) :: :ok | any()
  def configure_pin(pin, direction) do
    cond do
      pin > @max_pin -> {:error, :invalid_pin}
      direction not in [:input, :output] -> {:error, :invalid_direction}
      true ->
        if direction == :input do
          Nif.configure_pin(pin, @input_direction)
        else
          Nif.configure_pin(pin, @output_direction)
        end
    end
  end

  @doc """
  Reads the input signal of a `pin`.

  `pin` should be a value between 0 and 127, inclusive.
  """
  @spec read(non_neg_integer()) :: {:ok, level()} | any()
  def read(pin) do
    if pin > @max_pin do
      {:error, :invalid_pin}
    else
      case Nif.read(pin) do
        {:ok, @low_level} -> {:ok, :low}
        {:ok, @high_level} -> {:ok, :high}
      end
    end
  end

  @doc """
  Writes a signal of `level` out to the `pin`.

  `pin` should be a value between 0 and 127, inclusive. `level` should be
  either `:low` or `:high` for a low output signal and high output signal
  respectively.
  """
  @spec write(non_neg_integer(), level()) :: :ok | any()
  def write(pin, level) do
    cond do
      pin > @max_pin -> {:error, :invalid_pin}
      level not in [:low, :high] -> {:error, :invalid_direction}
      true ->
        if level == :low do
          Nif.write(pin, @low_level)
        else
          Nif.write(pin, @high_level)
        end
    end
  end

  @doc """
  Sets the debouncing functionality of a `pin`.

  `pin` should be a value between 0 and 127, inclusive. `debounce` should be
  `true` to turn debouncing on, `false` otherwise.
  """
  @spec set_debounce(non_neg_integer(), boolean()) :: :ok | any()
  def set_debounce(pin, debounce) do
    cond do
      pin > @max_pin -> {:error, :invalid_pin}
      true ->
        if debounce do
          Nif.set_debounce(pin, 1)
        else
          Nif.set_debounce(pin, 0)
        end
    end
  end

  @doc """
  Sets the debouncing timing for a particular `pin` and also for the other pins
  under the same controller. There are four controllers and each controller
  controls 32 pins.

  `pin` should be a value between 0 and 127, inclusive. `time` should be a
  value between 0 and 255, inclusive.

  Each increment of `time` adds a 31 microsecond long clock pulse for the
  debouncing. There is always a base of 31 microsecond for the debouncing even
  if `time` is set to 0.
  """
  @spec set_debounce_time(non_neg_integer(), non_neg_integer()) :: :ok | any()
  def set_debounce_time(pin, time) do
    cond do
      pin > @max_pin -> {:error, :invalid_pin}
      time > @max_debounce_time -> {:error, :debounce_timing_is_too_high}
      true ->
        Nif.set_debounce_time(pin, time)
    end
  end
end
