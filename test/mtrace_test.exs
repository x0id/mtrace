defmodule MtraceTest do
  use ExUnit.Case

  test "stats" do
    assert {m, c, r, f} = Mtrace.stats()
    assert is_integer(m)
    assert is_integer(c)
    assert is_integer(r)
    assert is_integer(f)
    assert :ok = Nif.malloc()
    assert {m_, c_, r_, f_} = Mtrace.stats()
    assert m_ - m >= 1
    assert c_ - c >= 0
    assert r_ - r >= 0
    assert f_ - f >= 0
  end
end
