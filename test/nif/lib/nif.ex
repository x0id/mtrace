defmodule Nif do
  @on_load :load_nifs

  def load_nifs do
    :code.priv_dir(:nif)
    |> :filename.join(:nif)
    |> :erlang.load_nif(0)
  end

  def malloc do
    :erlang.nif_error(:nif_not_loaded)
  end
end
