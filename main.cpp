#include "MatchingEngine.hpp"
#include "Order.hpp"
#include "UserManager.hpp"
#include "TradeLogger.hpp"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include <iomanip>

using namespace OrderMatchingEngine;

/**
 * @brief Demo functions to showcase the Order Matching Engine capabilities
 */
class OrderMatchingEngineDemo {
private:
    std::unique_ptr<MatchingEngine> engine;
    std::vector<std::string> symbols = {"AAPL", "GOOGL", "MSFT", "TSLA", "AMZN"};
    std::vector<std::string> userIds;
    std::mt19937 rng;

    void initializeUsers() {
        std::cout << "\n=== Initializing Users ===\n";

        for (int i = 1; i <= 10; ++i) {
            std::string userId = "USER_" + std::to_string(i);
            std::string userName = "Trader " + std::to_string(i);
            userIds.push_back(userId);

            // Users will be created automatically when first order is submitted
            std::cout << "User " << userId << " (" << userName << ") registered\n";
        }
    }

    OrderPtr createRandomOrder(const std::string& userId) {
        std::uniform_int_distribution<> symbolDist(0, symbols.size() - 1);
        std::uniform_int_distribution<> typeDist(0, 2);
        std::uniform_int_distribution<> sideDist(0, 1);
        std::uniform_real_distribution<> priceDist(100.0, 200.0);
        std::uniform_int_distribution<> qtyDist(10, 100);

        std::string symbol = symbols[symbolDist(rng)];
        OrderType type = static_cast<OrderType>(typeDist(rng));
        OrderSide side = static_cast<OrderSide>(sideDist(rng));
        double price = (type == OrderType::MARKET) ? 0.0 : priceDist(rng);
        int quantity = qtyDist(rng);

        static int orderCounter = 1;
        std::string orderId = "ORDER_" + std::to_string(orderCounter++);

        // For stop-loss orders, set appropriate trigger price
        double triggerPrice = 0.0;
        if (type == OrderType::STOP_LOSS) {
            std::uniform_real_distribution<> triggerDist(80.0, 120.0);
            triggerPrice = triggerDist(rng);
        }

        return std::make_shared<Order>(orderId, userId, symbol, type, side, 
                                     price, quantity, triggerPrice);
    }

    void simulateTrading(int numOrders, int delayMs = 100) {
        std::cout << "\n=== Simulating " << numOrders << " random orders ===\n";

        std::uniform_int_distribution<> userDist(0, userIds.size() - 1);

        for (int i = 0; i < numOrders; ++i) {
            std::string userId = userIds[userDist(rng)];
            auto order = createRandomOrder(userId);

            std::string submittedOrderId = engine->submitOrder(order);

            if (!submittedOrderId.empty()) {
                std::cout << "Order " << submittedOrderId << " submitted: "
                         << order->toString() << "\n";
            } else {
                std::cout << "Failed to submit order: " << order->toString() << "\n";
            }

            // Add some delay between orders
            if (delayMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }

            // Occasionally print engine status
            if ((i + 1) % 20 == 0) {
                std::cout << "\n--- Progress: " << (i + 1) << "/" << numOrders 
                         << " orders processed ---\n";
                engine->printStatus();
                std::cout << "\n";
            }
        }
    }

    void demonstrateOrderManagement() {
        std::cout << "\n=== Demonstrating Order Management ===\n";

        // Create some specific orders for demonstration
        std::string userId = userIds[0];

        // Create a buy limit order
        auto buyOrder = std::make_shared<Order>("DEMO_BUY_001", userId, "AAPL", 
                                               OrderType::LIMIT, OrderSide::BUY, 
                                               150.0, 100);
        std::string buyOrderId = engine->submitOrder(buyOrder);
        std::cout << "Submitted buy order: " << buyOrderId << "\n";

        // Create a sell limit order that won't match immediately
        auto sellOrder = std::make_shared<Order>("DEMO_SELL_001", userId, "AAPL",
                                                OrderType::LIMIT, OrderSide::SELL,
                                                160.0, 50);
        std::string sellOrderId = engine->submitOrder(sellOrder);
        std::cout << "Submitted sell order: " << sellOrderId << "\n";

        // Show order book state
        std::cout << "\nOrder book state after submissions:\n";
        auto orderBook = engine->getOrderBook("AAPL");
        if (orderBook) {
            orderBook->printOrderBook();
        }

        // Demonstrate order modification
        std::cout << "\nModifying buy order price to 155.0...\n";
        bool modified = engine->modifyOrder(buyOrderId, userId, 155.0, 0);
        std::cout << "Order modification " << (modified ? "successful" : "failed") << "\n";

        // Wait a moment then cancel the sell order
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "\nCancelling sell order...\n";
        bool cancelled = engine->cancelOrder(sellOrderId, userId);
        std::cout << "Order cancellation " << (cancelled ? "successful" : "failed") << "\n";
    }

    void demonstrateMarketData() {
        std::cout << "\n=== Market Data Overview ===\n";

        auto allMarketData = engine->getAllMarketData();

        std::cout << std::left << std::setw(8) << "Symbol"
                 << std::setw(12) << "Best Bid"
                 << std::setw(12) << "Best Ask"
                 << std::setw(12) << "Last Price"
                 << std::setw(10) << "Volume"
                 << std::setw(8) << "Trades"
                 << "Spread\n";
        std::cout << std::string(70, '-') << "\n";

        for (const auto& data : allMarketData) {
            std::cout << std::left << std::setw(8) << data.symbol
                     << std::setw(12) << std::fixed << std::setprecision(2) << data.bestBid
                     << std::setw(12) << data.bestAsk
                     << std::setw(12) << data.lastTradePrice
                     << std::setw(10) << std::setprecision(0) << data.totalVolume
                     << std::setw(8) << data.totalTrades
                     << std::setprecision(2) << data.spread << "\n";
        }
    }

public:
    OrderMatchingEngineDemo() : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {
        // Create a high-performance matching engine
        engine = MatchingEngineFactory::createHighPerformanceEngine();

        // Add symbols to the engine
        for (const auto& symbol : symbols) {
            engine->addSymbol(symbol);
        }

        initializeUsers();
    }

    void runDemo() {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "    ORDER MATCHING ENGINE DEMONSTRATION\n";
        std::cout << std::string(60, '=') << "\n";

        // Start the matching engine
        std::cout << "\nStarting matching engine...\n";
        engine->start();

        // Wait for engine to fully initialize
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Demonstrate basic order management
        demonstrateOrderManagement();

        // Simulate trading activity
        simulateTrading(50, 50);

        // Show market data
        demonstrateMarketData();

        // Show final engine statistics
        std::cout << "\n=== Final Engine Statistics ===\n";
        auto stats = engine->getStatistics();
        std::cout << stats.toString() << "\n";

        // Wait a moment before shutdown
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Stop the engine
        std::cout << "\nStopping matching engine...\n";
        engine->stop();

        std::cout << "\nDemo completed successfully!\n";
    }
};

/**
 * @brief Performance testing function
 */
void performanceTest() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "    PERFORMANCE TEST\n";
    std::cout << std::string(60, '=') << "\n";

    auto engine = MatchingEngineFactory::createHighPerformanceEngine();
    engine->addSymbol("PERF_TEST");
    engine->start();

    const int NUM_ORDERS = 10000;
    const std::string USER_ID = "PERF_USER";

    std::cout << "\nGenerating " << NUM_ORDERS << " orders for performance test...\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    // Submit orders as fast as possible
    for (int i = 0; i < NUM_ORDERS; ++i) {
        std::string orderId = "PERF_ORDER_" + std::to_string(i);
        OrderSide side = (i % 2 == 0) ? OrderSide::BUY : OrderSide::SELL;
        double price = 100.0 + (i % 20) - 10.0; // Price range 90-110
        int quantity = 10 + (i % 90); // Quantity range 10-100

        auto order = std::make_shared<Order>(orderId, USER_ID, "PERF_TEST",
                                           OrderType::LIMIT, side, price, quantity);

        engine->submitOrder(order);

        if ((i + 1) % 1000 == 0) {
            std::cout << "Progress: " << (i + 1) << "/" << NUM_ORDERS << " orders\n";
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    double ordersPerSecond = static_cast<double>(NUM_ORDERS) * 1000000.0 / duration.count();
    double avgLatencyMicros = static_cast<double>(duration.count()) / NUM_ORDERS;

    std::cout << "\nPerformance Test Results:\n";
    std::cout << "Total Orders: " << NUM_ORDERS << "\n";
    std::cout << "Total Time: " << duration.count() << " microseconds\n";
    std::cout << "Orders/Second: " << std::fixed << std::setprecision(2) << ordersPerSecond << "\n";
    std::cout << "Avg Latency: " << std::setprecision(3) << avgLatencyMicros << " microseconds\n";

    auto finalStats = engine->getStatistics();
    std::cout << "\nFinal Engine Stats:\n";
    std::cout << finalStats.toString() << "\n";

    engine->stop();
}

/**
 * @brief Interactive mode for manual testing
 */
void interactiveMode() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "    INTERACTIVE MODE\n";
    std::cout << std::string(60, '=') << "\n";

    auto engine = MatchingEngineFactory::createBasicEngine();
    engine->addSymbol("TEST");
    engine->start();

    std::string command;
    std::cout << "\nEnter commands (type 'help' for available commands):\n";

    while (true) {
        std::cout << "> ";
        std::getline(std::cin, command);

        if (command == "quit" || command == "exit") {
            break;
        } else if (command == "help") {
            std::cout << "Available commands:\n";
            std::cout << "  buy <price> <qty>   - Submit buy limit order\n";
            std::cout << "  sell <price> <qty>  - Submit sell limit order\n";
            std::cout << "  market buy <qty>    - Submit market buy order\n";
            std::cout << "  market sell <qty>   - Submit market sell order\n";
            std::cout << "  status              - Show engine status\n";
            std::cout << "  orderbook           - Show order book\n";
            std::cout << "  quit/exit           - Exit interactive mode\n";
        } else if (command == "status") {
            engine->printStatus();
        } else if (command == "orderbook") {
            auto orderBook = engine->getOrderBook("TEST");
            if (orderBook) {
                orderBook->printOrderBook();
            }
        } else {
            std::cout << "Unknown command. Type 'help' for available commands.\n";
        }
    }

    engine->stop();
}

/**
 * @brief Main function with menu system
 */
int main() {
    std::cout << "Order Matching Engine v1.0\n";
    std::cout << "Built with advanced data structures and algorithms\n\n";

    while (true) {
        std::cout << "Select an option:\n";
        std::cout << "1. Run Full Demo\n";
        std::cout << "2. Performance Test\n";
        std::cout << "3. Interactive Mode\n";
        std::cout << "4. Exit\n";
        std::cout << "Choice: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore(); // Consume newline

        switch (choice) {
            case 1: {
                OrderMatchingEngineDemo demo;
                demo.runDemo();
                break;
            }
            case 2: {
                performanceTest();
                break;
            }
            case 3: {
                interactiveMode();
                break;
            }
            case 4: {
                std::cout << "Goodbye!\n";
                return 0;
            }
            default: {
                std::cout << "Invalid choice. Please try again.\n\n";
                break;
            }
        }

        std::cout << "\nPress Enter to continue...";
        std::cin.get();
        std::cout << "\n";
    }

    return 0;
}