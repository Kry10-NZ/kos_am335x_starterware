# KosAm335xStarterware


## Manifest installation

Adding `kos_am335x_starterware` to your list of manifest dependencies in `mix.exs`:

```elixir
def deps do
  [
    {:kos_am335x_starterware, git: "https://github.com/Kry10-NZ/kos_am335x_starterware.git"}
  ]
end
```

Add a GPIO service to the project using `KosAm335xStarterware.Manifest.include_gpio`:
```

    {:ok, context, _app, _protocol} = KosAm335xStarterware.Manifest.include_gpio(context, protocol: "gpio_protocol")
```


Add a PWM service to the project using `KosAm335xStarterware.Manifest.include_pwm`:

```

    {:ok, context, _app, _protocol} = KosAm335xStarterware.Manifest.include_pwm(context, protocol: "pwm_protocol_0", am335x_pwm_id: 0)
```


## Module installation

Adding `kos_am335x_starterware` to your list of application modules in an application `mix.exs`:

```elixir
def deps do
  [
    {:kos_am335x_starterware, git: "https://github.com/Kry10-NZ/kos_am335x_starterware.git", sparse: "interface"}
  ]
end
```

The following example toggles one of the LEDs on the BeagleBone Black:

```elixir
  alias KosAm335xStarterware.GPIO

  @led_gpio 55

  defp blink_led() do
    {:ok, gpio_handle} = GPIO.setup(gpio_protocol: "gpio_protocol")
    :ok = GPIO.configure_pin(gpio_handle, @led_gpio, :output)
    blink(gpio_handle)
  end

  defp blink(gpio_handle) do
    :ok = GPIO.write(gpio_handle, @led_gpio, :high)
    :timer.sleep(1000)
    :ok = GPIO.write(gpio_handle, @led_gpio, :low)
    :timer.sleep(1000)
    blink(gpio_handle)
  end

```
