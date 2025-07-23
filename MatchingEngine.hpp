#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include "OrderBook.hpp"
#include "UserManager.hpp"
#include "TradeLogger.hpp"
#include <unordered_map>
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>

namespace OrderMatchingEngine {

/**
 * @brief Main matching engine class that orchestrates the entire order matching system
 * 
 * Features:
 * - Multi-symbol order book management
 * - High-performance order processing with lock-free algorithms
 * - Real-time market data distribution
 * - Risk management and validation
 * - Multi-threaded architecture for scalability
 */
class MatchingEngine {
public:
    // Core components
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> orderBooks;
    std::unique_ptr<UserManager> userManager;
    std::unique_ptr<TradeLogger> tradeLogger;

    // Threading and concurrency
    std::atomic<bool> running;
    std::vector<std::thread> workerThreads;
    mutable std::mutex engineMutex;
    std::condition_variable orderCondition;

    // Order processing queue (lock-free for high performance)
    struct OrderRequest {
        OrderPtr order;
        std::chrono::high_resolution_clock::time_point timestamp;
        int priority; // Higher values = higher priority

        OrderRequest(OrderPtr o, int p = 0) 
            : order(o), timestamp(std::chrono::high_resolution_clock::now()), priority(p) {}
    };

    // Priority queue for order requests
    std::priority_queue<OrderRequest, std::vector<OrderRequest>, 
                       std::function<bool(const OrderRequest&, const OrderRequest&)>> orderQueue;

    // Statistics and monitoring
    std::atomic<long long> totalOrdersProcessed;
    std::atomic<long long> totalTradesExecuted;
    std::atomic<double> totalVolumeTraded;
    std::chrono::high_resolution_clock::time_point startTime;

    // Configuration
    struct EngineConfig {
        int maxWorkerThreads;
        int maxQueueSize;
        bool enableRiskManagement;
        bool enableMarketDataBroadcast;
        double maxOrderSize;
        double maxPositionSize;
        int orderTimeoutSeconds;
        bool enableStopLossOrders;
        bool enableMultiThreading;

        EngineConfig() : maxWorkerThreads(4), maxQueueSize(10000), 
                        enableRiskManagement(true), enableMarketDataBroadcast(true),
                        maxOrderSize(1000000.0), maxPositionSize(5000000.0),
                        orderTimeoutSeconds(86400), enableStopLossOrders(true),
                        enableMultiThreading(true) {}
    } config;

    // Risk management
    struct RiskLimits {
        double maxDailyVolume;
        double maxPositionSize;
        double maxOrderSize;
        int maxOrdersPerSecond;
        std::unordered_map<std::string, double> symbolRiskFactors;

        RiskLimits() : maxDailyVolume(100000000.0), maxPositionSize(10000000.0),
                      maxOrderSize(1000000.0), maxOrdersPerSecond(1000) {}
    } riskLimits;

    // Market data subscribers
    std::vector<std::function<void(const Trade&)>> tradeCallbacks;
    std::vector<std::function<void(const std::string&, double, double)>> marketDataCallbacks;

    // Internal methods
    void workerThreadFunction();
    void processOrderRequest(const OrderRequest& request);
    bool validateOrder(const OrderPtr& order);
    bool checkRiskLimits(const OrderPtr& order);
    void notifyTradeExecuted(const Trade& trade);
    void notifyMarketDataUpdate(const std::string& symbol, double bid, double ask);
    void cleanupExpiredOrders();
    OrderBook* getOrCreateOrderBook(const std::string& symbol);

public:
    /**
     * @brief Constructor
     * @param config Engine configuration
     */
    explicit MatchingEngine(const EngineConfig& config = EngineConfig());

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~MatchingEngine();

    // Core order operations
    /**
     * @brief Submit a new order to the matching engine
     * @param order Order to be processed
     * @return Order ID if successfully submitted, empty string if failed
     */
    std::string submitOrder(OrderPtr order);

    /**
     * @brief Cancel an existing order
     * @param orderId ID of order to cancel
     * @param userId User requesting cancellation (for authorization)
     * @return True if successfully cancelled, false otherwise
     */
    bool cancelOrder(const std::string& orderId, const std::string& userId);

    /**
     * @brief Modify an existing order
     * @param orderId ID of order to modify
     * @param userId User requesting modification (for authorization)
     * @param newPrice New price (0 to keep current)
     * @param newQuantity New quantity (0 to keep current)
     * @return True if successfully modified, false otherwise
     */
    bool modifyOrder(const std::string& orderId, const std::string& userId,
                    double newPrice = 0.0, int newQuantity = 0);

    // Query operations
    /**
     * @brief Get order details
     */
    OrderPtr getOrder(const std::string& orderId) const;

    /**
     * @brief Get all orders for a user
     */
    std::vector<OrderPtr> getUserOrders(const std::string& userId) const;

    /**
     * @brief Get order book for a symbol
     */
    const OrderBook* getOrderBook(const std::string& symbol) const;

    /**
     * @brief Get current market data for a symbol
     */
    struct MarketData {
        std::string symbol;
        double bestBid;
        double bestAsk;
        double lastTradePrice;
        long long lastTradeTime;
        double totalVolume;
        long long totalTrades;
        double spread;

        std::string toString() const;
    };

    MarketData getMarketData(const std::string& symbol) const;

    /**
     * @brief Get market data for all symbols
     */
    std::vector<MarketData> getAllMarketData() const;

    // Engine control
    /**
     * @brief Start the matching engine
     */
    void start();

    /**
     * @brief Stop the matching engine
     */
    void stop();

    /**
     * @brief Check if engine is running
     */
    bool isRunning() const { return running.load(); }

    // Statistics and monitoring
    struct EngineStatistics {
        long long totalOrdersProcessed;
        long long totalTradesExecuted;
        double totalVolumeTraded;
        long long uptimeSeconds;
        int activeSymbols;
        int queueSize;
        double averageProcessingTimeMs;
        double ordersPerSecond;
        double tradesPerSecond;

        std::string toString() const;
    };

    EngineStatistics getStatistics() const;

    /**
     * @brief Print comprehensive engine status
     */
    void printStatus() const;

    // Configuration management
    /**
     * @brief Update engine configuration
     */
    void updateConfig(const EngineConfig& newConfig);

    /**
     * @brief Update risk limits
     */
    void updateRiskLimits(const RiskLimits& newLimits);

    // Event subscription
    /**
     * @brief Subscribe to trade events
     */
    void subscribeToTrades(std::function<void(const Trade&)> callback);

    /**
     * @brief Subscribe to market data updates
     */
    void subscribeToMarketData(std::function<void(const std::string&, double, double)> callback);

    // Utility methods
    /**
     * @brief Get all supported symbols
     */
    std::vector<std::string> getSupportedSymbols() const;

    /**
     * @brief Add support for new trading symbol
     */
    void addSymbol(const std::string& symbol);

    /**
     * @brief Remove support for trading symbol
     */
    void removeSymbol(const std::string& symbol);

    /**
     * @brief Reset all order books (for testing)
     */
    void reset();

    /**
     * @brief Export order book state to file
     */
    bool exportOrderBookState(const std::string& filename) const;

    /**
     * @brief Import order book state from file
     */
    bool importOrderBookState(const std::string& filename);

    // Advanced features
    /**
     * @brief Enable/disable circuit breaker for a symbol
     * Circuit breaker halts trading when price moves exceed threshold
     */
    void setCircuitBreaker(const std::string& symbol, double percentThreshold, int durationSeconds);

    /**
     * @brief Batch order submission for improved throughput
     */
    std::vector<std::string> submitBatchOrders(const std::vector<OrderPtr>& orders);

    /**
     * @brief Get order book depth for multiple symbols
     */
    std::unordered_map<std::string, std::vector<std::pair<double, int>>> 
    getMultiSymbolDepth(const std::vector<std::string>& symbols, int levels) const;
};

/**
 * @brief Factory class for creating matching engine instances
 */
class MatchingEngineFactory {
public:
    /**
     * @brief Create a basic matching engine instance
     */
    static std::unique_ptr<MatchingEngine> createBasicEngine();

    /**
     * @brief Create a high-performance matching engine instance
     */
    static std::unique_ptr<MatchingEngine> createHighPerformanceEngine();

    /**
     * @brief Create a testing matching engine instance
     */
    static std::unique_ptr<MatchingEngine> createTestingEngine();

    /**
     * @brief Create a matching engine with custom configuration
     */
    static std::unique_ptr<MatchingEngine> createCustomEngine(
        const MatchingEngine::EngineConfig& config);
};

/**
 * @brief Performance monitor for the matching engine
 */
class PerformanceMonitor {
private:
    std::atomic<long long> orderCount;
    std::atomic<long long> tradeCount;
    std::chrono::high_resolution_clock::time_point startTime;
    std::atomic<double> totalLatency;

public:
    PerformanceMonitor();

    void recordOrderProcessed(double latencyMs);
    void recordTradeExecuted();

    struct PerformanceMetrics {
        double averageLatencyMs;
        long long ordersPerSecond;
        long long tradesPerSecond;
        long long totalOrders;
        long long totalTrades;
        long long uptimeSeconds;
    };

    PerformanceMetrics getMetrics() const;
    void reset();
};

} // namespace OrderMatchingEngine

#endif // MATCHING_ENGINE_HPP