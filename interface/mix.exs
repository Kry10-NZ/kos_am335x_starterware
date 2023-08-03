# Copyright (c) 2023, Kry10 Limited. All rights reserved.
#
# SPDX-License-Identifier: LicenseRef-Kry10

defmodule KosAm335xStarterware.MixProject do
  use Mix.Project

  def project do
    [
      app: :kos_am335x_starterware,
      version: "0.1.0",
      elixir: "~> 1.11",
      start_permanent: Mix.env() == :prod,
      build_embedded: true,
      deps: deps()
    ]
  end

  # Run "mix help compile.app" to learn about applications.
  def application do
    [
      extra_applications: [:logger],
      mod: {KosAm335xStarterware, []}
    ]
  end

  # Run "mix help deps" to learn about dependencies.
  defp deps do
    kos_builtins = System.get_env("KOS_BUILTINS_PATH", "KOS_BUILTINS_PATH-NOTFOUND")
    [
      {:kos_msg, path: Path.join(kos_builtins, "/kos_msg_ex")}
    ]
  end
end
