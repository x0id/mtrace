defmodule Mtrace do
  @on_load :load_nifs

  def load_nifs do
    :code.priv_dir(:mtrace)
    |> :filename.join(:mtrace)
    |> :erlang.load_nif(0)
  end

  # def allocated do
  #   :erlang.nif_error(:nif_not_loaded)
  # end

  def stats do
    :erlang.nif_error(:nif_not_loaded)
  end
end
