#ifndef ORDER_HPP
#define ORDER_HPP

#include <string>
#include <chrono>
#include <memory>

namespace OrderMatchingEngine {

/**
 * @brief Enum representing the order type
 */
enum class OrderType {
    LIMIT,      // Limit order - execute at specified price or better
    MARKET,     // Market order - execute immediately at best available price
    STOP_LOSS   // Stop-loss order - convert to market order when trigger price reached
};

/**
 * @brief Enum representing the order side (Buy or Sell)
 */
enum class OrderSide {
    BUY,    // Buy order
    SELL    // Sell order
};

/**
 * @brief Enum representing the current status of an order
 */
enum class OrderStatus {
    PENDING,        // Order is waiting in the order book
    PARTIAL_FILL,   // Order is partially filled
    FILLED,         // Order is completely filled
    CANCELLED,      // Order has been cancelled
    REJECTED,       // Order was rejected
    TRIGGERED       // Stop-loss order has been triggered
};

/**
 * @brief Order class representing a trading order
 * 
 * This class encapsulates all the essential information about a trading order
 * including price, quantity, timestamps, and identification details.
 */
class Order {
private:
    std::string orderId;        // Unique identifier for the order
    std::string userId;         // User who placed the order
    std::string symbol;         // Trading symbol (e.g., "AAPL", "MSFT")
    OrderType type;             // Type of order (LIMIT, MARKET, STOP_LOSS)
    OrderSide side;             // Side of the order (BUY/SELL)
    double price;               // Price per unit (0 for market orders)
    int quantity;               // Number of shares/units
    int remainingQuantity;      // Remaining quantity to be filled
    OrderStatus status;         // Current status of the order
    long long timestamp;        // Creation timestamp (microseconds since epoch)
    double triggerPrice;        // Trigger price for stop-loss orders (0 if not applicable)

public:
    /**
     * @brief Constructor for creating a new order
     * 
     * @param orderId Unique identifier for the order
     * @param userId User placing the order
     * @param symbol Trading symbol
     * @param type Type of order
     * @param side Buy or sell
     * @param price Price per unit
     * @param quantity Number of units
     * @param triggerPrice Trigger price for stop-loss orders (default: 0.0)
     */
    Order(const std::string& orderId,
          const std::string& userId,
          const std::string& symbol,
          OrderType type,
          OrderSide side,
          double price,
          int quantity,
          double triggerPrice = 0.0);

    // Copy constructor
    Order(const Order& other);

    // Assignment operator
    Order& operator=(const Order& other);

    // Destructor
    ~Order();

    // Getters
    const std::string& getOrderId() const { return orderId; }
    const std::string& getUserId() const { return userId; }
    const std::string& getSymbol() const { return symbol; }
    OrderType getType() const { return type; }
    OrderSide getSide() const { return side; }
    double getPrice() const { return price; }
    int getQuantity() const { return quantity; }
    int getRemainingQuantity() const { return remainingQuantity; }
    OrderStatus getStatus() const { return status; }
    long long getTimestamp() const { return timestamp; }
    double getTriggerPrice() const { return triggerPrice; }

    // Setters
    void setPrice(double newPrice) { price = newPrice; }
    void setQuantity(int newQuantity);
    void setStatus(OrderStatus newStatus) { status = newStatus; }
    void setTriggerPrice(double newTriggerPrice) { triggerPrice = newTriggerPrice; }

    /**
     * @brief Reduces the remaining quantity after a partial or full fill
     * 
     * @param filledQuantity Amount that was filled
     * @return True if order is completely filled, false otherwise
     */
    bool fillOrder(int filledQuantity);

    /**
     * @brief Checks if the order can be matched with another order
     * 
     * @param other The other order to check compatibility with
     * @return True if orders can be matched, false otherwise
     */
    bool isCompatibleWith(const Order& other) const;

    /**
     * @brief Checks if this is a buy order
     */
    bool isBuy() const { return side == OrderSide::BUY; }

    /**
     * @brief Checks if this is a sell order
     */
    bool isSell() const { return side == OrderSide::SELL; }

    /**
     * @brief Checks if this is a limit order
     */
    bool isLimit() const { return type == OrderType::LIMIT; }

    /**
     * @brief Checks if this is a market order
     */
    bool isMarket() const { return type == OrderType::MARKET; }

    /**
     * @brief Checks if this is a stop-loss order
     */
    bool isStopLoss() const { return type == OrderType::STOP_LOSS; }

    /**
     * @brief Converts the order to a string representation
     */
    std::string toString() const;

    /**
     * @brief Comparison operators for priority queue ordering
     * Buy orders: Higher price has priority, then earlier timestamp
     * Sell orders: Lower price has priority, then earlier timestamp
     */
    bool operator<(const Order& other) const;
    bool operator>(const Order& other) const;
    bool operator==(const Order& other) const;

private:
    /**
     * @brief Generates current timestamp in microseconds
     */
    static long long getCurrentTimestamp();
};

/**
 * @brief Shared pointer type for Order objects
 */
using OrderPtr = std::shared_ptr<Order>;

/**
 * @brief Comparator for buy orders (max-heap - highest price first)
 */
struct BuyOrderComparator {
    bool operator()(const OrderPtr& a, const OrderPtr& b) const {
        if (a->getPrice() != b->getPrice()) {
            return a->getPrice() < b->getPrice(); // Higher price has priority
        }
        return a->getTimestamp() > b->getTimestamp(); // Earlier timestamp has priority
    }
};

/**
 * @brief Comparator for sell orders (min-heap - lowest price first)
 */
struct SellOrderComparator {
    bool operator()(const OrderPtr& a, const OrderPtr& b) const {
        if (a->getPrice() != b->getPrice()) {
            return a->getPrice() > b->getPrice(); // Lower price has priority
        }
        return a->getTimestamp() > b->getTimestamp(); // Earlier timestamp has priority
    }
};

} // namespace OrderMatchingEngine

#endif // ORDER_HPP