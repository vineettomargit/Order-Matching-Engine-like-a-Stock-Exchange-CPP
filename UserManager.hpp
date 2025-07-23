#ifndef USER_MANAGER_HPP
#define USER_MANAGER_HPP

#include "Order.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <set>

namespace OrderMatchingEngine {

/**
 * @brief User account information and portfolio management
 */
class UserAccount {
private:
    std::string userId;
    std::string userName;
    double cashBalance;
    std::unordered_map<std::string, int> positions; // symbol -> quantity
    std::set<std::string> activeOrderIds;
    double totalPnL; // Profit and Loss
    bool isActive;
    std::chrono::system_clock::time_point creationTime;
    std::chrono::system_clock::time_point lastActivityTime;

    // Risk management
    double maxPositionValue;
    double dailyLossLimit;
    double currentDayLoss;
    int maxOrdersPerDay;
    int todayOrderCount;

public:
    UserAccount(const std::string& userId, const std::string& userName, 
                double initialBalance = 100000.0);

    // Account management
    const std::string& getUserId() const { return userId; }
    const std::string& getUserName() const { return userName; }
    double getCashBalance() const { return cashBalance; }
    double getTotalPnL() const { return totalPnL; }
    bool getIsActive() const { return isActive; }

    void setActive(bool active) { isActive = active; }
    void updateLastActivity();

    // Portfolio management
    bool hasPosition(const std::string& symbol) const;
    int getPosition(const std::string& symbol) const;
    void updatePosition(const std::string& symbol, int quantity);
    double getPositionValue(const std::string& symbol, double currentPrice) const;
    std::unordered_map<std::string, int> getAllPositions() const { return positions; }

    // Cash management
    bool hasSufficientBalance(double amount) const;
    void reserveCash(double amount);
    void releaseCash(double amount);
    void debitCash(double amount);
    void creditCash(double amount);

    // Order tracking
    void addOrder(const std::string& orderId);
    void removeOrder(const std::string& orderId);
    bool hasOrder(const std::string& orderId) const;
    int getActiveOrderCount() const { return activeOrderIds.size(); }
    std::set<std::string> getActiveOrders() const { return activeOrderIds; }

    // Risk management
    bool canPlaceOrder(double orderValue) const;
    void updateDayLoss(double loss);
    void resetDayCounters();

    // Account statistics
    struct AccountStats {
        double totalPnL;
        double cashBalance;
        int totalPositions;
        int activeOrders;
        double portfolioValue;
        std::string toString() const;
    };

    AccountStats getAccountStats(const std::unordered_map<std::string, double>& currentPrices) const;

    std::string toString() const;
};

/**
 * @brief Manages all user accounts and their interactions with the trading system
 */
class UserManager {
private:
    std::unordered_map<std::string, std::unique_ptr<UserAccount>> users;
    mutable std::mutex userManagerMutex;

    // System-wide limits
    int maxUsersLimit;
    double systemCashLimit;
    bool enableAccountCreation;

    // Session management
    std::unordered_map<std::string, std::string> activeSessions; // sessionId -> userId
    std::unordered_map<std::string, std::chrono::system_clock::time_point> sessionExpiry;

    // Audit trail
    struct UserAction {
        std::string userId;
        std::string action;
        std::string details;
        std::chrono::system_clock::time_point timestamp;
    };
    std::vector<UserAction> auditTrail;

    void logUserAction(const std::string& userId, const std::string& action, 
                      const std::string& details = "");

public:
    /**
     * @brief Constructor
     */
    UserManager();

    /**
     * @brief Destructor
     */
    ~UserManager();

    // User account management
    /**
     * @brief Create a new user account
     */
    bool createUser(const std::string& userId, const std::string& userName,
                   double initialBalance = 100000.0);

    /**
     * @brief Get user account
     */
    UserAccount* getUser(const std::string& userId);
    const UserAccount* getUser(const std::string& userId) const;

    /**
     * @brief Delete user account
     */
    bool deleteUser(const std::string& userId);

    /**
     * @brief Check if user exists
     */
    bool userExists(const std::string& userId) const;

    /**
     * @brief Get all user IDs
     */
    std::vector<std::string> getAllUserIds() const;

    /**
     * @brief Get total number of users
     */
    int getUserCount() const;

    // Portfolio operations
    /**
     * @brief Update user position after trade execution
     */
    bool updateUserPosition(const std::string& userId, const std::string& symbol,
                           int quantityChange, double price);

    /**
     * @brief Check if user can afford an order
     */
    bool canUserAfford(const std::string& userId, const OrderPtr& order,
                      double currentPrice) const;

    /**
     * @brief Reserve funds for an order
     */
    bool reserveFundsForOrder(const std::string& userId, const OrderPtr& order,
                             double currentPrice);

    /**
     * @brief Release reserved funds when order is cancelled
     */
    void releaseFundsForOrder(const std::string& userId, const OrderPtr& order,
                             double currentPrice);

    // Session management
    /**
     * @brief Create user session
     */
    std::string createSession(const std::string& userId);

    /**
     * @brief Validate session
     */
    bool validateSession(const std::string& sessionId, std::string& userId);

    /**
     * @brief Terminate session
     */
    void terminateSession(const std::string& sessionId);

    /**
     * @brief Clean up expired sessions
     */
    void cleanupExpiredSessions();

    // Risk management
    /**
     * @brief Check if user meets risk requirements for order
     */
    bool checkRiskLimits(const std::string& userId, const OrderPtr& order) const;

    /**
     * @brief Update daily loss for user
     */
    void updateUserDayLoss(const std::string& userId, double loss);

    /**
     * @brief Reset daily counters for all users
     */
    void resetDailyCounters();

    // Reporting and analytics
    struct SystemStats {
        int totalUsers;
        int activeUsers;
        double totalCashInSystem;
        double totalPortfolioValue;
        int totalActiveOrders;
        std::string toString() const;
    };

    SystemStats getSystemStats(const std::unordered_map<std::string, double>& currentPrices) const;

    /**
     * @brief Get user portfolio summary
     */
    struct PortfolioSummary {
        std::string userId;
        double cashBalance;
        double portfolioValue;
        double totalPnL;
        int activeOrders;
        std::unordered_map<std::string, int> positions;
        std::string toString() const;
    };

    PortfolioSummary getUserPortfolio(const std::string& userId,
                                     const std::unordered_map<std::string, double>& currentPrices) const;

    /**
     * @brief Get all user portfolios
     */
    std::vector<PortfolioSummary> getAllPortfolios(
        const std::unordered_map<std::string, double>& currentPrices) const;

    // Order tracking
    /**
     * @brief Add order to user's active orders
     */
    bool addOrderToUser(const std::string& userId, const std::string& orderId);

    /**
     * @brief Remove order from user's active orders
     */
    bool removeOrderFromUser(const std::string& userId, const std::string& orderId);

    /**
     * @brief Check if user owns order
     */
    bool userOwnsOrder(const std::string& userId, const std::string& orderId) const;

    // System administration
    /**
     * @brief Enable/disable new account creation
     */
    void setAccountCreationEnabled(bool enabled);

    /**
     * @brief Set maximum users limit
     */
    void setMaxUsersLimit(int limit);

    /**
     * @brief Get audit trail for user
     */
    std::vector<UserAction> getUserAuditTrail(const std::string& userId) const;

    /**
     * @brief Export user data to file
     */
    bool exportUserData(const std::string& filename) const;

    /**
     * @brief Import user data from file
     */
    bool importUserData(const std::string& filename);

    /**
     * @brief Print system summary
     */
    void printSystemSummary(const std::unordered_map<std::string, double>& currentPrices) const;
};

} // namespace OrderMatchingEngine

#endif // USER_MANAGER_HPP