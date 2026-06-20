# Pinned ESP-IDF image for reproducible Mayhem MDK builds.
# Keep the parent image ENTRYPOINT (sources export.sh); only replace CMD.
FROM espressif/idf:v5.4.1

ENV IDF_COMPONENT_MANAGER=1

COPY build-in-container.sh /usr/local/bin/build-mdk.sh
RUN chmod +x /usr/local/bin/build-mdk.sh && sed -i 's/\r$//' /usr/local/bin/build-mdk.sh

WORKDIR /project
CMD ["/usr/local/bin/build-mdk.sh"]
