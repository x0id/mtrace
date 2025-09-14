defmodule MtraceTest do
  use ExUnit.Case

  test "stats" do
    assert {m, c, r, f} = Mtrace.stats()
    assert is_integer(m)
    assert is_integer(c)
    assert is_integer(r)
    assert is_integer(f)
  end
end
