defmodule MtraceTest do
  use ExUnit.Case

  test "stats" do
    assert %{} = x = Mtrace.stats()
    IO.inspect(x)

    for {k, v} <- x do
      assert is_atom(k)
      assert is_integer(v)
    end

    assert :ok = Nif.malloc()
    assert %{} = x_ = Mtrace.stats()
    assert x_[:malloc_cnt] - x[:malloc_cnt] >= 1
  end
end
