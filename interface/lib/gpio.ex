# Copyright (c) 2023, Kry10 Limited. All rights reserved.
#
# SPDX-License-Identifier: LicenseRef-Kry10

defmodule KosAm335xStarterware.GPIO do
  @moduledoc """
  Control AM335X GPIO pins in Elixir.
  """

  @typedoc "Directions allowed for the GPIO pins"
  @type direction :: :input | :output

  @typedoc "Input/output signal levels"
  @type level :: :low | :high

  @type handle :: %KosAm335xStarterware.GPIO{
    gpio_ref: reference()
  }

  defstruct [:gpio_ref]

  @gpio_prot "am335x_gpio_protocol"

  @max_pin 127
  @max_debounce_time 255

  @input_direction 0
  @output_direction 1

  @low_level 0
  @high_level 1

  @debounce_on 1
  @debounce_off 0

  @configure_pin_label 1
  @set_debounce_label 2
  @set_debounce_timing_label 3
  @read_label 4
  @write_label 5

  @doc """
  Performs initial setup to connect to the GPIO service.

  This returns a handle if setup was successful that needs to be passed to the
  other functions in this module.
  """
  @spec setup(Keyword.t()) :: {:ok, KosAm335xStarterware.GPIO.handle()} | {:error, any}
  def setup(opts \\ []) do
    prot = Keyword.get(opts, :gpio_protocol, @gpio_prot)

    case KosMsg.open(prot) do
      {:ok, gpio_ref} -> {:ok, %KosAm335xStarterware.GPIO{gpio_ref: gpio_ref}}
      {:error, _} -> {:error, :failed_to_setup_gpio}
    end
  end

  @doc """
  Configures the direction of a `pin`.

  `handle` should be the output given by `setup()/1`. `pin` should be a value
  between 0 and 127, inclusive. `direction` can be of either `:input` or
  `:output`.
  """
  @spec configure_pin(KosAm335xStarterware.GPIO.handle(), non_neg_integer(), direction()) :: :ok | any()
  def configure_pin(handle, pin, direction) do
    cond do
      pin > @max_pin -> {:error, :invalid_pin}
      direction not in [:input, :output] -> {:error, :invalid_direction}
      true ->
        direction_value = if direction == :input do
          @input_direction
        else
          @output_direction
        end

        data = [{:uint32_t, pin}, {:uint32_t, direction_value}]
        case call_gpio_server(handle.gpio_ref, data, @configure_pin_label) do
          {:ok, _} -> :ok
          error -> error
        end
    end
  end

  @doc """
  Reads the input signal of a `pin`.

  `handle` should be the output given by `setup()/1`. `pin` should be a value
  between 0 and 127, inclusive.
  """
  @spec read(KosAm335xStarterware.GPIO.handle(), non_neg_integer()) :: {:ok, level()} | any()
  def read(handle, pin) do
    cond do
      pin > @max_pin -> {:error, :invalid_pin}
      true ->
        data = [{:uint32_t, pin}]
        case call_gpio_server(handle.gpio_ref, data, @read_label) do
          {:ok, result} ->
            if :binary.decode_unsigned(result) == 0 do
              {:ok, :low}
            else
              {:ok, :high}
            end
          error -> error
        end
    end
  end

  @doc """
  Writes a signal of `level` out to the `pin`.

  `handle` should be the output given by `setup()/1`. `pin` should be a value
  between 0 and 127, inclusive. `level` should be either `:low` or `:high` for
  a low output signal and high output signal respectively.
  """
  @spec write(KosAm335xStarterware.GPIO.handle(), non_neg_integer(), level()) :: :ok | any()
  def write(handle, pin, level) do
    cond do
      pin > @max_pin -> {:error, :invalid_pin}
      level not in [:low, :high] -> {:error, :invalid_direction}
      true ->
        data = if level == :low do
          [{:uint32_t, pin}, {:uint32_t, @low_level}]
        else
          [{:uint32_t, pin}, {:uint32_t, @high_level}]
        end
        case call_gpio_server(handle.gpio_ref, data, @write_label) do
          {:ok, _} -> :ok
          error -> error
        end
    end
  end

  @doc """
  Sets the debouncing functionality of a `pin`.

  `handle` should be the output given by `setup()/1`. `pin` should be a value
  between 0 and 127, inclusive. `debounce` should be `true` to turn debouncing
  on, `false` otherwise.
  """
  @spec set_debounce(KosAm335xStarterware.GPIO.handle(), non_neg_integer(), boolean()) :: :ok | any()
  def set_debounce(handle, pin, debounce) do
    cond do
      pin > @max_pin -> {:error, :invalid_pin}
      true ->
        data = if debounce do
          [{:uint32_t, pin}, {:uint32_t, @debounce_on}]
        else
          [{:uint32_t, pin}, {:uint32_t, @debounce_off}]
        end
        case call_gpio_server(handle.gpio_ref, data, @set_debounce_label) do
          {:ok, _} -> :ok
          error -> error
        end
    end
  end

  @doc """
  Sets the debouncing timing for a particular `pin` and also for the other pins
  under the same controller. There are four controllers and each controller
  controls 32 pins.

  `handle` should be the output given by `setup()/1`. `pin` should be a value
  between 0 and 127, inclusive. `timing` should be a value between 0 and 255,
  inclusive.

  Each increment of `timing` adds a 31 microsecond long clock pulse for the
  debouncing. There is always a base of 31 microsecond for the debouncing even
  if `timing` is set to 0.
  """
  @spec set_debounce_timing(KosAm335xStarterware.GPIO.handle(), non_neg_integer(), non_neg_integer()) :: :ok | any()
  def set_debounce_timing(handle, pin, time) do
    cond do
      pin > @max_pin -> {:error, :invalid_pin}
      time > @max_debounce_time -> {:error, :debounce_timing_is_too_high}
      true ->
        data = [{:uint32_t, pin}, {:uint32_t, time}]
        case call_gpio_server(handle.gpio_ref, data, @set_debounce_timing_label) do
          {:ok, _} -> :ok
          error -> error
        end
    end
  end

  defp call_gpio_server(gpio_ref, data, request) do
    payload = KosMsg.encode(data)
    status_ok = KosMsg.status_ok()

    case KosMsg.call_msg(gpio_ref, request, 0, payload) do
      {:error, _} ->
        {:error, :failed_to_invoke_gpio_operation}
      {:ok, {^status_ok, _, result}} -> {:ok, result}
      {:ok, {_, _, _}} ->
        {:error, :failed_to_perform_gpio_operation}
    end
  end
end
