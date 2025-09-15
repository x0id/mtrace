defmodule Mtrace.MixProject do
  use Mix.Project

  def project do
    [
      app: :mtrace,
      version: "0.1.0",
      elixir: "~> 1.17",
      compilers: [:make | Mix.compilers()],
      aliases: [clean: ["clean.make", "clean"]],
      start_permanent: Mix.env() == :prod,
      deps: deps()
    ]
  end

  # Run "mix help compile.app" to learn about applications.
  def application do
    [
      extra_applications: [:logger],
      mod: {Mtrace.Application, []}
    ]
  end

  # Run "mix help deps" to learn about dependencies.
  defp deps do
    [
      {:nif, [path: "test/nif", only: :test]}
    ]
  end
end

defmodule Mix.Tasks.Compile.Make do
  use Mix.Task

  def run(_args) do
    target_dir = Mix.Project.app_path() |> Path.join("priv")
    {result, exit_code} = System.cmd("make", [], cd: "c_src", env: [{"TARGET_DIR", target_dir}])
    IO.binwrite(result)
    exit_code == 0 || Mix.raise("NIF compile failed")
    :ok
  end
end

defmodule Mix.Tasks.Clean.Make do
  use Mix.Task

  def run(_args) do
    {result, exit_code} = System.cmd("make", ["clean"], cd: "c_src")
    IO.binwrite(result)
    exit_code == 0 || Mix.raise("NIF clean failed")
    :ok
  end
end
