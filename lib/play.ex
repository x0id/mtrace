defmodule Play do
  def plot do
    Mtrace.batch()
    |> elem(1)
    |> Map.keys()
    |> Enum.reduce({%{}, %{}}, &proc/2)
    |> write()
  end

  defp proc(addr, acc) do
    Mtrace.stack(addr)
    |> elem(1)
    |> loop_calls(acc)
  end

  defp loop_calls([{addr, lib, sym}, {next, _, _} = call | calls], {m1, m2}) do
    m1_ = Map.put(m1, addr, {lib, sym})
    m2_ = Map.update(m2, {next, addr}, 1, &(&1 + 1))
    loop_calls([call | calls], {m1_, m2_})
  end

  defp loop_calls([{addr, lib, sym} | calls], {m1, m2}) do
    m1_ = Map.put(m1, addr, {lib, sym})
    loop_calls(calls, {m1_, m2})
  end

  defp loop_calls([nil | calls], acc) do
    loop_calls(calls, acc)
  end

  defp loop_calls([], acc) do
    acc
  end

  defp write({m1, m2}) do
    IO.puts("digraph {")

    Enum.each(m2, fn
      {{a, b}, _n} ->
        s = m1[a] |> tostr(a)
        d = m1[b] |> tostr(b)
        IO.puts("\"#{s}\" -> \"#{d}\"")
    end)

    IO.puts("}")
  end

  defp tostr({lib, sym}, _addr) do
    "#{lib}\n#{Mtrace.demangle(true, sym)}"
  end
end
