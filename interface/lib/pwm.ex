# Copyright (c) 2023, Kry10 Limited. All rights reserved.
#
# SPDX-License-Identifier: LicenseRef-Kry10

defmodule KosAm335xStarterware.PWM do
  @moduledoc """
  Control AM335X PWM pins in Elixir.

  Currently, the PWM is running without a prescaler on the input clock (100
  MHz). This has some effect on accuracy on some frequencies of the PWM signal
  and also imposes a minimum of 2 kHz of the output signal.
  """

  @typedoc "Controllable pins in the PWM controller on the AM335X"
  @type pwm_pin :: :pwm_a | :pwm_b

  @type handle :: %KosAm335xStarterware.PWM{
    pwm_ref: reference(),
  }

  defstruct [:pwm_ref, :curr_frequency]

  @pwm_prot "am335x_pwm_protocol"

  @pwm_a 0
  @pwm_b 1

  @max_frequency 100000000
  @min_frequency 2000
  @max_duty_cycle 100
  @max_counter_value 65535

  @set_pwm_frequency_label 1
  @set_pwm_duty_cycle_label 2

  @doc """
  Performs initial setup to connect to the PWM service.

  This returns a handle if setup was successful that needs to be passed to the
  other functions in this module.
  """
  @spec setup(Keyword.t()) :: {:ok, KosAm335xStarterware.PWM.handle()} | {:error, any}
  def setup(opts \\ []) do
    prot = Keyword.get(opts, :pwm_protocol, @pwm_prot)

    case KosMsg.open(prot) do
      {:ok, pwm_ref} -> {:ok, %KosAm335xStarterware.PWM{pwm_ref: pwm_ref}}
      {:error, _} -> {:error, :failed_to_setup_pwm}
    end
  end

  @doc """
  Sets the frequency of the PWM signals of the entire controller.

  `handle` should be the output given by `setup()/1`. `frequency` should be a
  value from 2000 (2 kHz) to 100000000 (100 MHz).

  This will also ensure that the duty cycle of the pins in the controller will
  be updated to match the new frequency.
  """
  @spec set_pwm_frequency(KosAm335xStarterware.PWM.handle(), non_neg_integer()) :: :ok | any()
  def set_pwm_frequency(handle, frequency) do
    cond do
      frequency > @max_frequency -> {:error, :frequency_is_too_high}
      frequency < @min_frequency -> {:error, :frequency_is_too_low}
      true ->
        data = [{:uint32_t, frequency}]
        case call_pwm_server(handle.pwm_ref, data, @set_pwm_frequency_label) do
          {:ok, _} -> :ok
          error -> error
        end
    end
  end

  @doc """
  Changes the duty cycle of a particular `pin` on the controller.

  `handle` should be the output given by `setup()/1`. `duty_cycle` should be a
  value from 0 to 100, i.e. duty cycle in percentage.
  """
  @spec set_pwm_duty_cycle(KosAm335xStarterware.PWM.handle, pwm_pin(), non_neg_integer()) :: :ok | any()
  def set_pwm_duty_cycle(handle, pin, duty_cycle) do
    cond do
      pin not in [:pwm_a, :pwm_b] -> {:error, :invalid_pin}
      duty_cycle > @max_duty_cycle -> {:error, :duty_cycle_is_too_high}
      true ->
        data = if pin == :pwm_a do
          [{:uint32_t, @pwm_a}, {:uint32_t, duty_cycle}]
        else
          [{:uint32_t, @pwm_b}, {:uint32_t, duty_cycle}]
        end
        case call_pwm_server(handle.pwm_ref, data, @set_pwm_duty_cycle_label) do
          {:ok, _} -> :ok
          error -> error
        end
    end
  end

  defp call_pwm_server(pwm_ref, data, request) do
    payload = KosMsg.encode(data)
    status_ok = KosMsg.status_ok()

    case KosMsg.call_msg(pwm_ref, request, 0, payload) do
      {:error, _} -> {:error, :failed_to_perform_pwm_operation}
      {:ok, {^status_ok, _, result}} -> {:ok, result}
      {:ok, {_, _, _}} -> {:error, :failed_to_perform_pwm_operation}
    end
  end
end
