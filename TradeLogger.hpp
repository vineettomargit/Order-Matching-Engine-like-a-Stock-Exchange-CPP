#ifndef TRADE_LOGGER_HPP
#define TRADE_LOGGER_HPP

#include "OrderBook.hpp"
#include <vector>
#include <fstream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <atomic>

namespace OrderMatchingEngine {

/**
 * @brief Log levels for different types of events
 */
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

/**
 * @brief Represents a log entry in the system
 */
struct LogEntry {
    LogLevel level;
    std::string category;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    std::string threadId;

    LogEntry(LogLevel level, const std::string& category, 
             const std::string& message);

    std::string toString() const;
    std::string toCSV() const;
    std::string toJSON() const;
};

/**
 * @brief High-performance asynchronous trade and system logger
 * 
 * Features:
 * - Asynchronous logging for minimal impact on trading performance
 * - Multiple output formats (CSV, JSON, plain text)
 * - Configurable log levels and filtering
 * - Automatic log rotation and archival
 * - Real-time trade audit trail
 * - System event logging
 */
class TradeLogger {
public:
    // Configuration
    struct LoggerConfig {
        std::string logDirectory;
        std::string tradeLogFile;
        std::string systemLogFile;
        LogLevel minLogLevel;
        bool enableConsoleOutput;
        bool enableFileOutput;
        bool enableAsyncLogging;
        int maxLogFileSize; // in MB
        int maxLogFiles;    // for rotation
        bool autoFlush;
        int flushIntervalSeconds;

        LoggerConfig() : logDirectory("./logs"), tradeLogFile("trades.csv"),
                        systemLogFile("system.log"), minLogLevel(LogLevel::INFO),
                        enableConsoleOutput(true), enableFileOutput(true),
                        enableAsyncLogging(true), maxLogFileSize(100),
                        maxLogFiles(10), autoFlush(true), flushIntervalSeconds(5) {}
    } config;

    // File streams
    std::unique_ptr<std::ofstream> tradeLogStream;
    std::unique_ptr<std::ofstream> systemLogStream;

    // Async logging
    std::atomic<bool> running;
    std::thread loggingThread;
    std::queue<LogEntry> logQueue;
    std::mutex logMutex;
    std::condition_variable logCondition;

    // Statistics
    std::atomic<long long> totalTradesLogged;
    std::atomic<long long> totalEventsLogged;
    std::chrono::system_clock::time_point startTime;

    // Trade history for analytics
    std::vector<Trade> tradeHistory;
    mutable std::mutex tradeHistoryMutex;
    int maxTradeHistory;

    // Performance metrics
    std::atomic<double> averageLoggingLatencyMs;
    std::atomic<long long> loggingLatencyCount;

    // Internal methods
    void loggingThreadFunction();
    void processLogEntry(const LogEntry& entry);
    void writeToFile(const LogEntry& entry);
    void writeToConsole(const LogEntry& entry);
    void rotateLogFile(const std::string& filename);
    void initializeLogFiles();
    void flushLogFiles();
    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) const;
    std::string getCurrentThreadId() const;

public:
    /**
     * @brief Constructor
     */
    explicit TradeLogger(const LoggerConfig& config = LoggerConfig());

    /**
     * @brief Destructor - ensures all logs are flushed
     */
    ~TradeLogger();

    // Core logging methods
    /**
     * @brief Log a trade execution
     */
    void logTrade(const Trade& trade);

    /**
     * @brief Log multiple trades (batch operation)
     */
    void logTrades(const std::vector<Trade>& trades);

    /**
     * @brief Log a system event
     */
    void logEvent(LogLevel level, const std::string& category, 
                  const std::string& message);

    /**
     * @brief Log order events
     */
    void logOrderSubmitted(const OrderPtr& order);
    void logOrderCancelled(const std::string& orderId, const std::string& reason);
    void logOrderModified(const std::string& orderId, const std::string& changes);
    void logOrderFilled(const std::string& orderId, int quantity, double price);
    void logOrderRejected(const OrderPtr& order, const std::string& reason);

    /**
     * @brief Log system events
     */
    void logSystemStart();
    void logSystemStop();
    void logEngineStateChange(const std::string& oldState, const std::string& newState);
    void logError(const std::string& component, const std::string& error);
    void logWarning(const std::string& component, const std::string& warning);
    void logInfo(const std::string& component, const std::string& info);
    void logDebug(const std::string& component, const std::string& debug);

    // Query and analytics methods
    /**
     * @brief Get trade history within date range
     */
    std::vector<Trade> getTradeHistory(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) const;

    /**
     * @brief Get trades for specific symbol
     */
    std::vector<Trade> getTradesForSymbol(const std::string& symbol) const;

    /**
     * @brief Get trades for specific user (by order IDs)
     */
    std::vector<Trade> getTradesForUser(const std::string& userId) const;

    /**
     * @brief Get trade statistics
     */
    struct TradeStatistics {
        long long totalTrades;
        double totalVolume;
        double averageTradeSize;
        std::unordered_map<std::string, long long> tradesPerSymbol;
        std::unordered_map<std::string, double> volumePerSymbol;
        std::chrono::system_clock::time_point firstTrade;
        std::chrono::system_clock::time_point lastTrade;

        std::string toString() const;
    };

    TradeStatistics getTradeStatistics() const;

    /**
     * @brief Get trade statistics for specific symbol
     */
    TradeStatistics getSymbolStatistics(const std::string& symbol) const;

    /**
     * @brief Get daily trade summary
     */
    struct DailyTradeSummary {
        std::string date;
        long long totalTrades;
        double totalVolume;
        double highPrice;
        double lowPrice;
        double openPrice;
        double closePrice;
        double vwapPrice; // Volume Weighted Average Price

        std::string toString() const;
    };

    std::vector<DailyTradeSummary> getDailyTradeSummary(
        const std::string& symbol,
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) const;

    // Configuration and control
    /**
     * @brief Update logger configuration
     */
    void updateConfig(const LoggerConfig& newConfig);

    /**
     * @brief Set minimum log level
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief Enable/disable console output
     */
    void setConsoleOutput(bool enabled);

    /**
     * @brief Enable/disable file output
     */
    void setFileOutput(bool enabled);

    /**
     * @brief Force flush all pending logs
     */
    void flush();

    /**
     * @brief Start the logger (if using async mode)
     */
    void start();

    /**
     * @brief Stop the logger
     */
    void stop();

    // Export and reporting
    /**
     * @brief Export trade data to CSV file
     */
    bool exportTradesToCSV(const std::string& filename,
                          const std::chrono::system_clock::time_point& start,
                          const std::chrono::system_clock::time_point& end) const;

    /**
     * @brief Export trade data to JSON file
     */
    bool exportTradesToJSON(const std::string& filename,
                           const std::chrono::system_clock::time_point& start,
                           const std::chrono::system_clock::time_point& end) const;

    /**
     * @brief Generate trading report
     */
    bool generateTradingReport(const std::string& filename,
                              const std::string& symbol = "") const;

    /**
     * @brief Archive old log files
     */
    void archiveLogs(int daysOld = 30);

    // Performance monitoring
    struct LoggerPerformance {
        double averageLoggingLatencyMs;
        long long totalEventsLogged;
        long long totalTradesLogged;
        long long queueSize;
        bool isAsyncMode;
        double eventsPerSecond;
        long long uptimeSeconds;

        std::string toString() const;
    };

    LoggerPerformance getPerformanceMetrics() const;

    /**
     * @brief Reset performance counters
     */
    void resetPerformanceCounters();

    // Utility methods
    /**
     * @brief Check if logger is running
     */
    bool isRunning() const { return running.load(); }

    /**
     * @brief Get current log queue size
     */
    int getQueueSize() const;

    /**
     * @brief Print logger status
     */
    void printStatus() const;
};

/**
 * @brief Singleton logger instance for global access
 */
class GlobalLogger {
private:
    static std::unique_ptr<TradeLogger> instance;
    static std::once_flag initFlag;

    static void initLogger();

public:
    static TradeLogger& getInstance();
    static void initialize(const TradeLogger::LoggerConfig& config);
    static void shutdown();
};

// Convenience macros for logging
#define LOG_TRADE(trade) GlobalLogger::getInstance().logTrade(trade)
#define LOG_ERROR(component, message) GlobalLogger::getInstance().logError(component, message)
#define LOG_WARNING(component, message) GlobalLogger::getInstance().logWarning(component, message)
#define LOG_INFO(component, message) GlobalLogger::getInstance().logInfo(component, message)
#define LOG_DEBUG(component, message) GlobalLogger::getInstance().logDebug(component, message)

} // namespace OrderMatchingEngine

#endif // TRADE_LOGGER_HPP