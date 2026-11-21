#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

// Типи подій на виробничій лінії (варіант 17)
enum class ProductionEventType {
    PartManufactured,         // Виготовлення деталі
    QualityControlCompleted   // Завершення контролю якості
};

// Структура даних про подію
struct ProductionEvent {
    ProductionEventType type;
    int productId;
    std::string productName;
    std::string status;
    double deviationValue;
    std::string strategyName;
};

// Абстрактний інтерфейс спостерігача (Observer)
class IProductionObserver {
public:
    virtual ~IProductionObserver() = default;
    virtual void update(const ProductionEvent& event) = 0;
};

// Видавець подій (Subject) - зберігає підписників через std::weak_ptr (RAII)
class ProductionEventManager {
private:
    std::vector<std::weak_ptr<IProductionObserver>> observers;

public:
    void subscribe(const std::shared_ptr<IProductionObserver>& observer) {
        if (observer) {
            observers.push_back(observer);
        }
    }

    void unsubscribe(const std::shared_ptr<IProductionObserver>& observer) {
        std::erase_if(observers, [&](const std::weak_ptr<IProductionObserver>& item) {
            auto ptr = item.lock();
            return !ptr || ptr == observer;
            });
    }

    void notify(const ProductionEvent& event) {
        // Автоматично очищаємо знищені об'єкти перед сповіщенням
        std::erase_if(observers, [](const std::weak_ptr<IProductionObserver>& item) {
            return item.expired();
            });

        for (const auto& item : observers) {
            if (auto observer = item.lock()) {
                observer->update(event);
            }
        }
    }

    size_t getActiveObserversCount() {
        std::erase_if(observers, [](const std::weak_ptr<IProductionObserver>& item) {
            return item.expired();
            });
        return observers.size();
    }
};

// Підписник 1: Журнал виробництва
class ProductionJournal : public IProductionObserver {
private:
    std::vector<std::string> logs;

public:
    void update(const ProductionEvent& event) override {
        std::string entry;
        if (event.type == ProductionEventType::PartManufactured) {
            entry = "[Journal] Manufactured part #" + std::to_string(event.productId) +
                " (" + event.productName + ") | Status: " + event.status;
        }
        else {
            entry = "[Journal] Quality control finished for part #" + std::to_string(event.productId) +
                " (" + event.productName + ") | Result: " + event.status +
                " | " + event.strategyName + " = " + std::to_string(event.deviationValue);
        }
        logs.push_back(entry);
        std::cout << entry << std::endl;
    }

    const std::vector<std::string>& getLogs() const {
        return logs;
    }
};

// Підписник 2: Статистика якості
class QualityStatistics : public IProductionObserver {
private:
    int manufacturedCount = 0;
    int acceptedCount = 0;
    int defectiveCount = 0;

public:
    void update(const ProductionEvent& event) override {
        if (event.type == ProductionEventType::PartManufactured) {
            manufacturedCount++;
            std::cout << "[Statistics] Total manufactured parts: " << manufacturedCount << std::endl;
        }
        else if (event.type == ProductionEventType::QualityControlCompleted) {
            if (event.status == "Accepted") {
                acceptedCount++;
            }
            else if (event.status == "Defective") {
                defectiveCount++;
            }
            std::cout << "[Statistics] Quality updated -> Accepted: " << acceptedCount
                << ", Defective: " << defectiveCount << std::endl;
        }
    }

    int getManufacturedCount() const { return manufacturedCount; }
    int getAcceptedCount() const { return acceptedCount; }
    int getDefectiveCount() const { return defectiveCount; }

    void printSummary() const {
        int checked = acceptedCount + defectiveCount;
        double passRate = (checked > 0) ? (static_cast<double>(acceptedCount) / checked * 100.0) : 0.0;
        std::cout << "\n Quality Statistics Summary " << std::endl;
        std::cout << "Manufactured: " << manufacturedCount << std::endl;
        std::cout << "Accepted:     " << acceptedCount << std::endl;
        std::cout << "Defective:    " << defectiveCount << std::endl;
        std::cout << "Pass Rate:    " << passRate << "%" << std::endl;
        std::cout << "----\n" << std::endl;
    }
};