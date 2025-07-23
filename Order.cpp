#include "Order.hpp"
#include <sstream>
#include <stdexcept>

namespace OrderMatchingEngine {

Order::Order(const std::string& orderId,
             const std::string& userId,
             const std::string& symbol,
             OrderType type,
             OrderSide side,
             double price,
             int quantity,
             double triggerPrice)
    : orderId(orderId), userId(userId), symbol(symbol), type(type), side(side),
      price(price), quantity(quantity), remainingQuantity(quantity),
      status(OrderStatus::PENDING), timestamp(getCurrentTimestamp()),
      triggerPrice(triggerPrice) {

    // Validation
    if (orderId.empty()) {
        throw std::invalid_argument("Order ID cannot be empty");
    }
    if (userId.empty()) {
        throw std::invalid_argument("User ID cannot be empty");
    }
    if (symbol.empty()) {
        throw std::invalid_argument("Symbol cannot be empty");
    }
    if (quantity <= 0) {
        throw std::invalid_argument("Quantity must be positive");
    }
    if (type == OrderType::LIMIT && price <= 0) {
        throw std::invalid_argument("Limit order price must be positive");
    }
    if (type == OrderType::STOP_LOSS && triggerPrice <= 0) {
        throw std::invalid_argument("Stop-loss trigger price must be positive");
    }
}

Order::Order(const Order& other)
    : orderId(other.orderId), userId(other.userId), symbol(other.symbol),
      type(other.type), side(other.side), price(other.price),
      quantity(other.quantity), remainingQuantity(other.remainingQuantity),
      status(other.status), timestamp(other.timestamp),
      triggerPrice(other.triggerPrice) {
}

Order& Order::operator=(const Order& other) {
    if (this != &other) {
        orderId = other.orderId;
        userId = other.userId;
        symbol = other.symbol;
        type = other.type;
        side = other.side;
        price = other.price;
        quantity = other.quantity;
        remainingQuantity = other.remainingQuantity;
        status = other.status;
        timestamp = other.timestamp;
        triggerPrice = other.triggerPrice;
    }
    return *this;
}

Order::~Order() {
    // Nothing to clean up explicitly
}

void Order::setQuantity(int newQuantity) {
    if (newQuantity <= 0) {
        throw std::invalid_argument("Quantity must be positive");
    }
    quantity = newQuantity;
    remainingQuantity = newQuantity;
}

bool Order::fillOrder(int filledQuantity) {
    if (filledQuantity <= 0 || filledQuantity > remainingQuantity) {
        throw std::invalid_argument("Invalid filled quantity");
    }

    remainingQuantity -= filledQuantity;

    if (remainingQuantity == 0) {
        status = OrderStatus::FILLED;
        return true;
    } else {
        status = OrderStatus::PARTIAL_FILL;
        return false;
    }
}

bool Order::isCompatibleWith(const Order& other) const {
    // Orders must be for the same symbol
    if (symbol != other.symbol) {
        return false;
    }

    // Orders must be on opposite sides
    if (side == other.side) {
        return false;
    }

    // Both orders must have remaining quantity
    if (remainingQuantity <= 0 || other.remainingQuantity <= 0) {
        return false;
    }

    // Orders must be in pending or partial fill status
    if (status == OrderStatus::FILLED || status == OrderStatus::CANCELLED ||
        other.status == OrderStatus::FILLED || other.status == OrderStatus::CANCELLED) {
        return false;
    }

    // Price compatibility check
    if (isBuy() && other.isSell()) {
        // Buy order price >= Sell order price
        if (isMarket() || other.isMarket()) {
            return true; // Market orders match with any price
        }
        return price >= other.price;
    } else if (isSell() && other.isBuy()) {
        // Sell order price <= Buy order price
        if (isMarket() || other.isMarket()) {
            return true; // Market orders match with any price
        }
        return price <= other.price;
    }

    return false;
}

std::string Order::toString() const {
    std::stringstream ss;
    ss << "Order[ID=" << orderId 
       << ", User=" << userId 
       << ", Symbol=" << symbol
       << ", Type=" << (type == OrderType::LIMIT ? "LIMIT" : 
                       type == OrderType::MARKET ? "MARKET" : "STOP_LOSS")
       << ", Side=" << (side == OrderSide::BUY ? "BUY" : "SELL")
       << ", Price=" << price
       << ", Qty=" << quantity
       << ", Remaining=" << remainingQuantity
       << ", Status=" << static_cast<int>(status)
       << ", Timestamp=" << timestamp;
    if (triggerPrice > 0) {
        ss << ", TriggerPrice=" << triggerPrice;
    }
    ss << "]";
    return ss.str();
}

bool Order::operator<(const Order& other) const {
    if (side == OrderSide::BUY) {
        // For buy orders: higher price has priority, then earlier timestamp
        if (price != other.price) {
            return price < other.price; // Reverse for max-heap behavior
        }
        return timestamp > other.timestamp; // Earlier timestamp has priority
    } else {
        // For sell orders: lower price has priority, then earlier timestamp
        if (price != other.price) {
            return price > other.price; // Normal for min-heap behavior
        }
        return timestamp > other.timestamp; // Earlier timestamp has priority
    }
}

bool Order::operator>(const Order& other) const {
    return other < *this;
}

bool Order::operator==(const Order& other) const {
    return orderId == other.orderId;
}

long long Order::getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

} // namespace OrderMatchingEngine