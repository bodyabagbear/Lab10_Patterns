#pragma once
#include "Equipment.h"
#include "SQLiteEquipmentRepository.h"
#include <memory>
#include <string>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>

// допоміжна функція отримання поточного часу для запису історії переходів
inline std::string getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

class ProductContext;

// Абстрактний інтерфейс стану виробу (State)
class IProductState {
public:
    virtual ~IProductState() = default;
    virtual bool inspect(ProductContext& context) = 0;
    virtual bool accept(ProductContext& context) = 0;
    virtual bool markDefective(ProductContext& context) = 0;
    virtual std::string getName() const = 0;
};

// Контекст виробу: він буде зберігати поточний стан (unique_ptr) і синхронізувати зміни з БД
class ProductContext {
private:
    Product product;
    SQLiteEquipmentRepository& repo;
    std::unique_ptr<IProductState> state;

public:
    ProductContext(const Product& prod,
        SQLiteEquipmentRepository& repository,
        std::unique_ptr<IProductState> initialState)
        : product(prod), repo(repository), state(std::move(initialState)) {
    }

    // Виконання переходу з атомарним оновленням БД та історії у транзакції SQLite
    bool transitionTo(std::unique_ptr<IProductState> newState) {
        std::string oldStatus = state->getName();
        std::string newStatus = newState->getName();

        bool saved = repo.updateProductStatusWithHistory(
            product.id, oldStatus, newStatus, getCurrentTimestamp()
        );

        if (!saved) {
            std::cout << "[Error] Failed to save state transition to DB!" << std::endl;
            return false;
        }

        product.status = newStatus;
        state = std::move(newState);
        return true;
    }

    bool inspect() { return state->inspect(*this); }
    bool accept() { return state->accept(*this); }
    bool markDefective() { return state->markDefective(*this); }

    std::string getStateName() const { return state->getName(); }
    const Product& getProduct() const { return product; }
};

// Стан 1: Виготовлена (дозволений тільки перехід у "Перевірена")
class ManufacturedState : public IProductState {
public:
    bool inspect(ProductContext& context) override;

    bool accept(ProductContext&) override {
        std::cout << "[State Blocked] Cannot accept part: it must be inspected first!" << std::endl;
        return false;
    }

    bool markDefective(ProductContext&) override {
        std::cout << "[State Blocked] Cannot mark defective: it must be inspected first!" << std::endl;
        return false;
    }

    std::string getName() const override { return "Manufactured"; }
};

// Стан 2: Перевірена (дозволений перехід у "Прийнята" або "Бракована")
class InspectedState : public IProductState {
public:
    bool inspect(ProductContext&) override {
        std::cout << "[State Blocked] Part is already inspected!" << std::endl;
        return false;
    }

    bool accept(ProductContext& context) override;
    bool markDefective(ProductContext& context) override;

    std::string getName() const override { return "Inspected"; }
};

// Стан 3: Прийнята (фінальний стан - подальші переходи заборонені)
class AcceptedState : public IProductState {
public:
    bool inspect(ProductContext&) override {
        std::cout << "[State Blocked] Part is already accepted (final state)!" << std::endl;
        return false;
    }

    bool accept(ProductContext&) override {
        std::cout << "[State Blocked] Part is already accepted!" << std::endl;
        return false;
    }

    bool markDefective(ProductContext&) override {
        std::cout << "[State Blocked] Cannot mark an accepted part as defective!" << std::endl;
        return false;
    }

    std::string getName() const override { return "Accepted"; }
};

// Стан 4: Бракована (окремий фінальний стан - подальші переходи заборонені)
class DefectiveState : public IProductState {
public:
    bool inspect(ProductContext&) override {
        std::cout << "[State Blocked] Part is already defective (final state)!" << std::endl;
        return false;
    }

    bool accept(ProductContext&) override {
        std::cout << "[State Blocked] Cannot accept a defective part!" << std::endl;
        return false;
    }

    bool markDefective(ProductContext&) override {
        std::cout << "[State Blocked] Part is already marked as defective!" << std::endl;
        return false;
    }

    std::string getName() const override { return "Defective"; }
};

// Реалізація дозволених переходів між станами
inline bool ManufacturedState::inspect(ProductContext& context) {
    return context.transitionTo(std::make_unique<InspectedState>());
}

inline bool InspectedState::accept(ProductContext& context) {
    return context.transitionTo(std::make_unique<AcceptedState>());
}

inline bool InspectedState::markDefective(ProductContext& context) {
    return context.transitionTo(std::make_unique<DefectiveState>());
}