# Copyright (c) 2023, Kry10 Limited. All rights reserved.
#
# SPDX-License-Identifier: LicenseRef-Kry10

defmodule KosAm335xStarterwareGpio.MixProject do
  use Mix.Project

  def project do
    [
      app: :kos_am335x_starterware_gpio,
      version: "0.1.0",
      elixir: "~> 1.11",
      compilers: [:elixir_make] ++ Mix.compilers,
      start_permanent: Mix.env() == :prod,
      build_embedded: true,
      deps: deps()
    ]
  end

  # Run "mix help deps" to learn about dependencies.
  defp deps do
    [
      {:elixir_make, "~> 0.6"},
    ]
  end
end
