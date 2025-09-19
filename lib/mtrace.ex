defmodule Mtrace do
  @on_load :load_nifs

  def load_nifs do
    :code.priv_dir(:mtrace)
    |> :filename.join(:mtrace)
    |> :erlang.load_nif(0)
  end

  def batch do
    :erlang.nif_error(:nif_not_loaded)
  end

  def erase(_) do
    :erlang.nif_error(:nif_not_loaded)
  end

  def reset do
    :erlang.nif_error(:nif_not_loaded)
  end

  def stack(_) do
    :erlang.nif_error(:nif_not_loaded)
  end

  def stats do
    :erlang.nif_error(:nif_not_loaded)
  end

  def malloc(_) do
    :erlang.nif_error(:nif_not_loaded)
  end

  def free(_) do
    :erlang.nif_error(:nif_not_loaded)
  end

  def vsn do
    :erlang.nif_error(:nif_not_loaded)
  end

  def stacks do
    batch()
    |> elem(1)
    |> Map.keys()
    |> Enum.map(&stack/1)
  end

  def olds(n \\ 10, p \\ false) do
    now = System.system_time(:second)

    batch()
    |> elem(1)
    |> Enum.sort_by(&elem(elem(&1, 1), 1), :asc)
    |> Enum.take(n)
    |> Enum.map(fn {addr, {size, ts}} ->
      {elapsed, calls} = stack(addr)
      {now - ts, size, addr, elapsed, calls |> pretty(p)}
    end)
  end

  def pretty(stack, true) when is_list(stack) do
    Enum.map(stack, fn
      nil ->
        nil

      {x, y} ->
        {to_string(x), demangle(y)}
    end)
  end

  def pretty(stack, _), do: stack

  def demangle(nil), do: nil

  def demangle(x) do
    System.cmd("sh", ["-c", "echo #{x} |c++filt"], lines: 1) |> elem(0)
  end

  def f do
    {_t1, addr} = Mtrace.malloc(1010)
    Process.sleep(10000)
    _t2 = Mtrace.free(addr)
    # IO.puts("#{t1} #{t2}")
  end

  def loop() do
    Process.spawn(&f/0, [])
    Process.sleep(1)
    loop()
  end
end
