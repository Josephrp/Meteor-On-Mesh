# Meshtonic LWD standalone — HackRF via SoapySDR + web UI on port 5000.
FROM python:3.12-slim-bookworm

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    soapysdr-tools \
    python3-soapysdr \
    soapysdr-module-hackrf \
    hackrf \
    libfftw3-dev \
    gcc \
    g++ \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt
COPY . .

ENV LORA_CONFIG=/app/lora-meshtonic.toml
ENV PYTHONUNBUFFERED=1

EXPOSE 5000
CMD ["python", "run/meshtonic_app.py", "--host", "0.0.0.0", "--port", "5000", "--no-browser"]
