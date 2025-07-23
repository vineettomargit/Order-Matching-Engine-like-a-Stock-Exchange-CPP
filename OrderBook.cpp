#include "OrderBook.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>

namespace OrderMatchingEngine {

// Trade implementation
Trade::Trade(const std::string& tradeId, const std::string& buyOrderId,
             const std::string& sellOrderId, const std::string& symbol,
             double price, int quantity)
    : tradeId(tradeId), buyOrderId(buyOrderId), sellOrderId(sellOrderId),
      symbol(symbol), price(price), quantity(quantity),
      timestamp(std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch()).count()) {
}

std::string Trade::toString() const {
    std::stringstream ss;
    ss << "Trade[ID=" << tradeId 
       << ", BuyOrder=" << buyOrderId 
       << ", SellOrder=" << sellOrderId
       << ", Symbol=" << symbol
       << ", Price=" << std::fixed << std::setprecision(2) << price
       << ", Quantity=" << quantity
       << ", Timestamp=" << timestamp << "]";
    return ss.str();
}

// PriceLevel implementation
PriceLevel::PriceLevel(double price) : price(price), totalQuantity(0), orderCount(0) {
}

void PriceLevel::addOrder(const OrderPtr& order) {
    orders.push(order);
    totalQuantity += order->getRemainingQuantity();
    orderCount++;
}

OrderPtr PriceLevel::removeOrder(const std::string& orderId) {
    std::queue<OrderPtr> tempQueue;
    OrderPtr foundOrder = nullptr;

    while (!orders.empty()) {
        auto order = orders.front();
        orders.pop();

        if (order->getOrderId() == orderId) {
            foundOrder = order;
            totalQuantity -= order->getRemainingQuantity();
            orderCount--;
        } else {
            tempQueue.push(order);
        }
    }

    orders = tempQueue;
    return foundOrder;
}

OrderPtr PriceLevel::getFirstOrder() {
    return orders.empty() ? nullptr : orders.front();
}

bool PriceLevel::isEmpty() const {
    return orders.empty();
}

std::vector<OrderPtr> PriceLevel::getAllOrders() const {
    std::vector<OrderPtr> result;
    std::queue<OrderPtr> tempQueue = orders;

    while (!tempQueue.empty()) {
        result.push_back(tempQueue.front());
        tempQueue.pop();
    }

    return result;
}

// AVL Tree implementation
template<typename T, typename Compare>
AVLTree<T, Compare>::AVLTree() : root(nullptr) {
}

template<typename T, typename Compare>
AVLTree<T, Compare>::~AVLTree() {
    // Smart pointers will handle cleanup automatically
}

template<typename T, typename Compare>
int AVLTree<T, Compare>::getHeight(std::shared_ptr<Node> node) {
    return node ? node->height : 0;
}

template<typename T, typename Compare>
int AVLTree<T, Compare>::getBalance(std::shared_ptr<Node> node) {
    return node ? getHeight(node->left) - getHeight(node->right) : 0;
}

template<typename T, typename Compare>
std::shared_ptr<typename AVLTree<T, Compare>::Node> 
AVLTree<T, Compare>::rotateRight(std::shared_ptr<Node> y) {
    auto x = y->left;
    auto T2 = x->right;

    x->right = y;
    y->left = T2;

    y->height = std::max(getHeight(y->left), getHeight(y->right)) + 1;
    x->height = std::max(getHeight(x->left), getHeight(x->right)) + 1;

    return x;
}

template<typename T, typename Compare>
std::shared_ptr<typename AVLTree<T, Compare>::Node> 
AVLTree<T, Compare>::rotateLeft(std::shared_ptr<Node> x) {
    auto y = x->right;
    auto T2 = y->left;

    y->left = x;
    x->right = T2;

    x->height = std::max(getHeight(x->left), getHeight(x->right)) + 1;
    y->height = std::max(getHeight(y->left), getHeight(y->right)) + 1;

    return y;
}

template<typename T, typename Compare>
std::shared_ptr<typename AVLTree<T, Compare>::Node> 
AVLTree<T, Compare>::insert(std::shared_ptr<Node> node, const T& data) {
    // Standard BST insertion
    if (!node) {
        return std::make_shared<Node>(data);
    }

    if (comp(data, node->data)) {
        node->left = insert(node->left, data);
    } else if (comp(node->data, data)) {
        node->right = insert(node->right, data);
    } else {
        return node; // Equal keys not allowed
    }

    // Update height
    node->height = 1 + std::max(getHeight(node->left), getHeight(node->right));

    // Get balance factor
    int balance = getBalance(node);

    // Left Left Case
    if (balance > 1 && comp(data, node->left->data)) {
        return rotateRight(node);
    }

    // Right Right Case
    if (balance < -1 && comp(node->right->data, data)) {
        return rotateLeft(node);
    }

    // Left Right Case
    if (balance > 1 && comp(node->left->data, data)) {
        node->left = rotateLeft(node->left);
        return rotateRight(node);
    }

    // Right Left Case
    if (balance < -1 && comp(data, node->right->data)) {
        node->right = rotateRight(node->right);
        return rotateLeft(node);
    }

    return node;
}

template<typename T, typename Compare>
void AVLTree<T, Compare>::insert(const T& data) {
    root = insert(root, data);
}

template<typename T, typename Compare>
bool AVLTree<T, Compare>::empty() const {
    return root == nullptr;
}

// OrderBook implementation
OrderBook::OrderBook(const std::string& symbol) 
    : symbol(symbol), totalTrades(0), totalVolume(0.0), lastTradePrice(0.0) {
}

OrderBook::~OrderBook() {
    // Smart pointers will handle cleanup
}

std::vector<Trade> OrderBook::addOrder(const OrderPtr& order) {
    std::lock_guard<std::mutex> lock(orderBookMutex);

    if (!order) {
        throw std::invalid_argument("Order cannot be null");
    }

    if (order->getSymbol() != symbol) {
        throw std::invalid_argument("Order symbol does not match order book symbol");
    }

    std::vector<Trade> trades;

    // Handle stop-loss orders
    if (order->isStopLoss()) {
        if (order->isBuy()) {
            buyStopLossOrders.insert(order);
        } else {
            sellStopLossOrders.insert(order);
        }
        addToOrderBook(order);
        return trades; // Stop-loss orders don't immediately match
    }

    // Attempt to match the order
    trades = matchOrder(order);

    // If the order is not completely filled, add it to the order book
    if (order->getRemainingQuantity() > 0) {
        addToOrderBook(order);
    }

    // Check if any stop-loss orders should be triggered
    if (!trades.empty()) {
        checkStopLossOrders(trades.back().price);
    }

    return trades;
}

std::vector<Trade> OrderBook::matchOrder(const OrderPtr& incomingOrder) {
    std::vector<Trade> trades;

    if (incomingOrder->isBuy()) {
        // Match buy order against sell orders
        while (!sellOrders.empty() && incomingOrder->getRemainingQuantity() > 0) {
            auto bestSellOrder = sellOrders.top();

            // Check if orders can be matched
            if (!incomingOrder->isCompatibleWith(*bestSellOrder)) {
                break;
            }

            sellOrders.pop();

            // Determine trade price and quantity
            double tradePrice = bestSellOrder->getPrice();
            if (bestSellOrder->isMarket()) {
                tradePrice = incomingOrder->isMarket() ? lastTradePrice : incomingOrder->getPrice();
            }

            int tradeQuantity = std::min(incomingOrder->getRemainingQuantity(),
                                       bestSellOrder->getRemainingQuantity());

            // Execute the trade
            Trade trade = executeTrade(incomingOrder, bestSellOrder, tradePrice, tradeQuantity);
            trades.push_back(trade);

            // If sell order still has remaining quantity, put it back
            if (bestSellOrder->getRemainingQuantity() > 0) {
                sellOrders.push(bestSellOrder);
            } else {
                removeFromOrderBook(bestSellOrder->getOrderId());
            }
        }
    } else {
        // Match sell order against buy orders
        while (!buyOrders.empty() && incomingOrder->getRemainingQuantity() > 0) {
            auto bestBuyOrder = buyOrders.top();

            // Check if orders can be matched
            if (!incomingOrder->isCompatibleWith(*bestBuyOrder)) {
                break;
            }

            buyOrders.pop();

            // Determine trade price and quantity
            double tradePrice = bestBuyOrder->getPrice();
            if (bestBuyOrder->isMarket()) {
                tradePrice = incomingOrder->isMarket() ? lastTradePrice : incomingOrder->getPrice();
            }

            int tradeQuantity = std::min(incomingOrder->getRemainingQuantity(),
                                       bestBuyOrder->getRemainingQuantity());

            // Execute the trade
            Trade trade = executeTrade(bestBuyOrder, incomingOrder, tradePrice, tradeQuantity);
            trades.push_back(trade);

            // If buy order still has remaining quantity, put it back
            if (bestBuyOrder->getRemainingQuantity() > 0) {
                buyOrders.push(bestBuyOrder);
            } else {
                removeFromOrderBook(bestBuyOrder->getOrderId());
            }
        }
    }

    return trades;
}

Trade OrderBook::executeTrade(const OrderPtr& buyOrder, const OrderPtr& sellOrder,
                             double price, int quantity) {
    // Fill both orders
    buyOrder->fillOrder(quantity);
    sellOrder->fillOrder(quantity);

    // Generate trade
    std::string tradeId = generateTradeId();
    Trade trade(tradeId, buyOrder->getOrderId(), sellOrder->getOrderId(),
                symbol, price, quantity);

    // Update statistics
    totalTrades++;
    totalVolume += quantity;
    lastTradePrice = price;

    return trade;
}

void OrderBook::addToOrderBook(const OrderPtr& order) {
    orderMap[order->getOrderId()] = order;
    userOrders[order->getUserId()].push_back(order);

    if (order->isBuy()) {
        buyOrders.push(order);
    } else {
        sellOrders.push(order);
    }
}

void OrderBook::removeFromOrderBook(const std::string& orderId) {
    auto it = orderMap.find(orderId);
    if (it != orderMap.end()) {
        auto order = it->second;
        orderMap.erase(it);

        // Remove from user orders
        auto& userOrdersList = userOrders[order->getUserId()];
        userOrdersList.erase(
            std::remove_if(userOrdersList.begin(), userOrdersList.end(),
                          [&orderId](const OrderPtr& o) { 
                              return o->getOrderId() == orderId; 
                          }),
            userOrdersList.end());
    }
}

bool OrderBook::cancelOrder(const std::string& orderId) {
    std::lock_guard<std::mutex> lock(orderBookMutex);

    auto it = orderMap.find(orderId);
    if (it == orderMap.end()) {
        return false;
    }

    auto order = it->second;
    order->setStatus(OrderStatus::CANCELLED);
    removeFromOrderBook(orderId);

    return true;
}

OrderPtr OrderBook::getOrder(const std::string& orderId) const {
    std::lock_guard<std::mutex> lock(orderBookMutex);

    auto it = orderMap.find(orderId);
    return it != orderMap.end() ? it->second : nullptr;
}

std::vector<OrderPtr> OrderBook::getUserOrders(const std::string& userId) const {
    std::lock_guard<std::mutex> lock(orderBookMutex);

    auto it = userOrders.find(userId);
    return it != userOrders.end() ? it->second : std::vector<OrderPtr>();
}

double OrderBook::getBestBid() const {
    std::lock_guard<std::mutex> lock(orderBookMutex);
    return buyOrders.empty() ? 0.0 : buyOrders.top()->getPrice();
}

double OrderBook::getBestAsk() const {
    std::lock_guard<std::mutex> lock(orderBookMutex);
    return sellOrders.empty() ? 0.0 : sellOrders.top()->getPrice();
}

double OrderBook::getSpread() const {
    double bid = getBestBid();
    double ask = getBestAsk();
    return (bid > 0 && ask > 0) ? ask - bid : 0.0;
}

OrderBook::OrderBookStats OrderBook::getStatistics() const {
    std::lock_guard<std::mutex> lock(orderBookMutex);

    OrderBookStats stats;
    stats.totalTrades = totalTrades;
    stats.totalVolume = totalVolume;
    stats.lastTradePrice = lastTradePrice;
    stats.totalBuyOrders = buyOrders.size();
    stats.totalSellOrders = sellOrders.size();
    stats.bestBid = getBestBid();
    stats.bestAsk = getBestAsk();
    stats.spread = getSpread();

    return stats;
}

void OrderBook::printOrderBook() const {
    std::lock_guard<std::mutex> lock(orderBookMutex);

    std::cout << "\n=== Order Book for " << symbol << " ===\n";
    std::cout << "Best Bid: " << getBestBid() << ", Best Ask: " << getBestAsk() << "\n";
    std::cout << "Spread: " << getSpread() << "\n";
    std::cout << "Total Buy Orders: " << buyOrders.size() << "\n";
    std::cout << "Total Sell Orders: " << sellOrders.size() << "\n";
    std::cout << "Last Trade Price: " << lastTradePrice << "\n";
    std::cout << "Total Trades: " << totalTrades << "\n";
    std::cout << "Total Volume: " << totalVolume << "\n";
    std::cout << "================================\n\n";
}

bool OrderBook::isEmpty() const {
    std::lock_guard<std::mutex> lock(orderBookMutex);
    return buyOrders.empty() && sellOrders.empty();
}

void OrderBook::checkStopLossOrders(double currentPrice) {
    // Implementation for checking and triggering stop-loss orders
    // This would iterate through stop-loss orders and convert them to market orders
    // when their trigger prices are hit
}

std::string OrderBook::generateTradeId() {
    static std::atomic<long long> tradeIdCounter(0);
    return "TRADE_" + symbol + "_" + std::to_string(++tradeIdCounter);
}

} // namespace OrderMatchingEngine