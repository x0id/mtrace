defmodule Nif.MixProject do
  use Mix.Project

  def project do
    [
      app: :nif,
      version: "0.1.0",
      elixir: "~> 1.17",
      compilers: [:make | Mix.compilers()],
      aliases: [clean: ["clean.make", "clean"]],
      start_permanent: Mix.env() == :prod
    ]
  end
end
