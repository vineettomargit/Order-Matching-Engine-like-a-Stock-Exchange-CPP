#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include "Order.hpp"
#include <queue>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <map>
#include <set>


namespace OrderMatchingEngine {

/**
 * @brief Represents a trade execution result
 */
struct Trade {
    std::string tradeId;
    std::string buyOrderId;
    std::string sellOrderId;
    std::string symbol;
    double price;
    int quantity;
    long long timestamp;

    Trade(const std::string& tradeId, const std::string& buyOrderId,
          const std::string& sellOrderId, const std::string& symbol,
          double price, int quantity);

    std::string toString() const;
};

/**
 * @brief Price level for maintaining orders at specific price points
 * Uses AVL-like balanced approach for efficient insertion/deletion
 */
class PriceLevel {
private:
    double price;
    int totalQuantity;
    int orderCount;
    std::queue<OrderPtr> orders; // FIFO queue for time priority

public:
    explicit PriceLevel(double price);

    void addOrder(const OrderPtr& order);
    OrderPtr removeOrder(const std::string& orderId);
    OrderPtr getFirstOrder();
    bool isEmpty() const;

    double getPrice() const { return price; }
    int getTotalQuantity() const { return totalQuantity; }
    int getOrderCount() const { return orderCount; }

    std::vector<OrderPtr> getAllOrders() const;
};

/**
 * @brief Custom Red-Black Tree implementation for price levels
 * Provides O(log n) insertion, deletion, and search operations
 */
template<typename Comparator>
class PriceLevelTree {
private:
    enum class Color { RED, BLACK };

    struct Node {
        std::shared_ptr<PriceLevel> priceLevel;
        std::shared_ptr<Node> left;
        std::shared_ptr<Node> right;
        std::shared_ptr<Node> parent;
        Color color;

        Node(std::shared_ptr<PriceLevel> level) 
            : priceLevel(level), left(nullptr), right(nullptr), 
              parent(nullptr), color(Color::RED) {}
    };

    std::shared_ptr<Node> root;
    Comparator comp;

    void rotateLeft(std::shared_ptr<Node> node);
    void rotateRight(std::shared_ptr<Node> node);
    void insertFixup(std::shared_ptr<Node> node);
    void deleteFixup(std::shared_ptr<Node> node);
    std::shared_ptr<Node> minimum(std::shared_ptr<Node> node);
    std::shared_ptr<Node> successor(std::shared_ptr<Node> node);

public:
    PriceLevelTree();
    ~PriceLevelTree();

    void insert(std::shared_ptr<PriceLevel> priceLevel);
    void remove(double price);
    std::shared_ptr<PriceLevel> find(double price);
    std::shared_ptr<PriceLevel> getBest(); // Get best price level
    bool empty() const;

    std::vector<std::shared_ptr<PriceLevel>> getAllLevels() const;
};

/**
 * @brief Specialized AVL Tree for fast order book operations
 * Maintains balance factor and provides guaranteed O(log n) operations
 */
template<typename T, typename Compare>
class AVLTree {
private:
    struct Node {
        T data;
        std::shared_ptr<Node> left;
        std::shared_ptr<Node> right;
        int height;

        Node(const T& value) : data(value), left(nullptr), right(nullptr), height(1) {}
    };

    std::shared_ptr<Node> root;
    Compare comp;

    int getHeight(std::shared_ptr<Node> node);
    int getBalance(std::shared_ptr<Node> node);
    std::shared_ptr<Node> rotateRight(std::shared_ptr<Node> y);
    std::shared_ptr<Node> rotateLeft(std::shared_ptr<Node> x);
    std::shared_ptr<Node> insert(std::shared_ptr<Node> node, const T& data);
    std::shared_ptr<Node> remove(std::shared_ptr<Node> node, const T& data);
    std::shared_ptr<Node> findMin(std::shared_ptr<Node> node);

public:
    AVLTree();
    ~AVLTree();

    void insert(const T& data);
    void remove(const T& data);
    T* find(const T& data);
    T* getMin();
    T* getMax();
    bool empty() const;

    std::vector<T> inOrderTraversal() const;
};

/**
 * @brief Order Book class managing buy and sell orders for a specific symbol
 * 
 * Uses advanced data structures:
 * - Priority queues (heaps) for fast best price access
 * - Hash maps for O(1) order lookup by ID
 * - AVL trees for maintaining price levels
 * - Red-Black trees for balanced price level management
 */
class OrderBook {
private:
    std::string symbol;

    // Priority queues for fast access to best prices
    std::priority_queue<OrderPtr, std::vector<OrderPtr>, BuyOrderComparator> buyOrders;
    std::priority_queue<OrderPtr, std::vector<OrderPtr>, SellOrderComparator> sellOrders;

    // Hash maps for O(1) order lookup and modification
    std::unordered_map<std::string, OrderPtr> orderMap;
    std::unordered_map<std::string, std::vector<OrderPtr>> userOrders;

    // Price level trees for efficient range queries and market depth
    PriceLevelTree<std::greater<double>> buyPriceLevels;  // Higher prices first
    PriceLevelTree<std::less<double>> sellPriceLevels;    // Lower prices first

    // // AVL trees for stop-loss order management
    // AVLTree<OrderPtr, [](const OrderPtr& a, const OrderPtr& b) { 
    //     return a->getTriggerPrice() < b->getTriggerPrice(); 
    // }> buyStopLossOrders;
    // AVLTree<OrderPtr, [](const OrderPtr& a, const OrderPtr& b) { 
    //     return a->getTriggerPrice() > b->getTriggerPrice(); 
    // }> sellStopLossOrders;

    struct StopLossBuyComparator
    {
        bool operator()(const OrderPtr &a, const OrderPtr &b) const
        {
            return a->getTriggerPrice() < b->getTriggerPrice();
        }
    };

    struct StopLossSellComparator
    {
        bool operator()(const OrderPtr &a, const OrderPtr &b) const
        {
            return a->getTriggerPrice() > b->getTriggerPrice();
        }
    };
    AVLTree<OrderPtr, StopLossBuyComparator> buyStopLossOrders;
    AVLTree<OrderPtr, StopLossSellComparator> sellStopLossOrders;

    // Thread safety
    mutable std::mutex orderBookMutex;

    // Statistics
    long long totalTrades;
    double totalVolume;
    double lastTradePrice;

    // Internal helper methods
    std::vector<Trade> matchOrder(const OrderPtr& incomingOrder);
    Trade executeTrade(const OrderPtr& buyOrder, const OrderPtr& sellOrder, 
                      double price, int quantity);
    void addToOrderBook(const OrderPtr& order);
    void removeFromOrderBook(const std::string& orderId);
    void updatePriceLevels(const OrderPtr& order);
    void checkStopLossOrders(double currentPrice);
    std::string generateTradeId();

public:
    /**
     * @brief Constructor
     * @param symbol Trading symbol for this order book
     */
    explicit OrderBook(const std::string& symbol);

    /**
     * @brief Destructor
     */
    ~OrderBook();

    // Core order operations
    /**
     * @brief Add a new order to the order book
     * @param order Order to be added
     * @return Vector of trades executed as a result of this order
     */
    std::vector<Trade> addOrder(const OrderPtr& order);

    /**
     * @brief Cancel an existing order
     * @param orderId ID of the order to cancel
     * @return True if order was successfully cancelled, false otherwise
     */
    bool cancelOrder(const std::string& orderId);

    /**
     * @brief Modify an existing order
     * @param orderId ID of the order to modify
     * @param newPrice New price (0 to keep current price)
     * @param newQuantity New quantity (0 to keep current quantity)
     * @return Vector of trades if modification triggers matching
     */
    std::vector<Trade> modifyOrder(const std::string& orderId, 
                                  double newPrice = 0, int newQuantity = 0);

    // Query operations
    /**
     * @brief Get order by ID
     */
    OrderPtr getOrder(const std::string& orderId) const;

    /**
     * @brief Get all orders for a specific user
     */
    std::vector<OrderPtr> getUserOrders(const std::string& userId) const;

    /**
     * @brief Get current best bid (highest buy price)
     */
    double getBestBid() const;

    /**
     * @brief Get current best ask (lowest sell price)  
     */
    double getBestAsk() const;

    /**
     * @brief Get bid-ask spread
     */
    double getSpread() const;

    /**
     * @brief Get market depth up to specified levels
     */
    std::vector<std::pair<double, int>> getMarketDepth(int levels, bool buySize) const;

    /**
     * @brief Get order book statistics
     */
    struct OrderBookStats {
        long long totalTrades;
        double totalVolume;
        double lastTradePrice;
        int totalBuyOrders;
        int totalSellOrders;
        double bestBid;
        double bestAsk;
        double spread;
    };

    OrderBookStats getStatistics() const;

    // Display methods
    /**
     * @brief Print current order book state
     */
    void printOrderBook() const;

    /**
     * @brief Get symbol this order book handles
     */
    const std::string& getSymbol() const { return symbol; }

    /**
     * @brief Check if order book is empty
     */
    bool isEmpty() const;
};

// Comparators for price levels
struct BuyPriceLevelComparator {
    bool operator()(double a, double b) const {
        return a < b; // Higher prices first for buy orders
    }
};

struct SellPriceLevelComparator {
    bool operator()(double a, double b) const {
        return a > b; // Lower prices first for sell orders
    }
};

} // namespace OrderMatchingEngine

#endif // ORDERBOOK_HPP