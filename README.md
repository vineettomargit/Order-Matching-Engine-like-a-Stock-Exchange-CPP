# Order-Matching-Engine-like-a-Stock-Exchange-CPP
Order Matching Engine like a Stock Exchange


. **Order Types Supported**:

   * **Limit Order**: Trades at a specific price or better.
   * **Market Order**: Executes at the best available price immediately.
   * **Stop-loss Order**: Becomes a market order after a trigger price is reached.

2. **Multithreaded Processing**:

   * Incoming client requests (order placements/modifications/cancellations) are processed asynchronously using a **ThreadSafeQueue**.
   * A dedicated worker thread runs the matching engine in the background.

3. **gRPC-based API**:

   * Clients can place, cancel, or modify orders over gRPC.

4. **Matching Rules**:

   * **Price-Time Priority**: Higher priced buy orders and lower priced sell orders take precedence.
   * **Partial Matching**: Orders can be partially filled and reinserted with the remaining quantity.

5. **Extensible Design**:

   * Decoupled `MatchingEngine`, `OrderBook`, and `Order` models.
   * Clean separation of protocol (gRPC) from business logic (engine core).
