FROM elixir:1.17.3-otp-25-slim AS builder

RUN apt-get update -qq
RUN apt-get install --no-install-recommends -y build-essential
