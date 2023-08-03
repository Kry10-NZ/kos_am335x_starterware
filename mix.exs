# Copyright (c) 2023, Kry10 Limited. All rights reserved.
#
# SPDX-License-Identifier: LicenseRef-Kry10

defmodule KosAm335xStarterware.MixProject do
  use Mix.Project

  def project do
    [
      app: :kos_am335x_starterware,
      compilers: [:cmake] ++ Mix.compilers,
      version: "0.1.0",
      deps: deps()
    ]
  end

  # Run "mix help compile.app" to learn about applications.
  def application do
    [
      extra_applications: [:logger, :kos_manifest],
    ]
  end

  defp deps do
    [
      {:elixir_cmake, "~> 0.8.0"}
    ]
  end
end
