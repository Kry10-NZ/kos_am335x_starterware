# Copyright (c) 2022, Kry10 Limited. All rights reserved.
#
# SPDX-License-Identifier: LicenseRef-Kry10

defmodule KosAm335xStarterware do
  use Application

  @impl true
  def start(_type, _args) do
    # start the application with the KosMsg genserver
    children = [KosMsg]

    Supervisor.start_link(children, strategy: :one_for_one)
  end
end
