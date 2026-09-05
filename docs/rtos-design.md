# RTOS Design Scaffold

The ownership model is handle-based. `AcqTask` owns DMA descriptors and is the
only task allowed to publish or recycle them. A live segment is retained for
each successfully queued decoder, and the last consumer release returns it to
the acquisition pool. Queue-full policy is drop-newest for decoded events and
explicit acquisition stop/overflow for exhausted DMA segments.

| Object | Owner | Initial full policy |
|---|---|---|
| DMA descriptor ring | AcqTask | stop and report overflow |
| decoder segment queues | AcqTask publishes, decoder consumes | release failed reference |
| event block pool | decoder producers, TxTask consumer | drop event and count |
| event queue | multiple decoder producers, TxTask consumer | drop block and count |
| UART RX StreamBuffer | UART RX ISR writes, CmdTask reads | parser-visible overflow |
| UART TX StreamBuffer | TxTask writes, UART TX ISR reads | bounded producer wait |

Windows tests use the portable ownership adapter in `fw-zynq/tests/host`.
FreeRTOS synchronization and interrupt timing remain target gates.
