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
      # {:dep_from_hexpm, "~> 0.3.0"},
      # {:dep_from_git, git: "https://github.com/elixir-lang/my_dep.git", tag: "0.1.0"}
    ]
  end
end

defmodule Mix.Tasks.Compile.Make do
  use Mix.Task

  def run(_args) do
    {result, exit_code} = System.cmd("make", [], cd: "c_src")
    IO.binwrite(result)
    exit_code == 0 || Mix.raise("NIF compile failed")

    # copy .so to _build/ to ensure module can find it when loaded by compiler
    Mix.Project.build_path()
    |> Path.join("lib/mtrace/priv")
    |> tap(&File.mkdir_p!/1)
    |> then(&File.cp("priv/mtrace.so", "#{&1}/mtrace.so"))

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
