#pragma once
#include "SQLiteEquipmentRepository.h"
#include "ProductState.h"
#include "DeviationStrategy.h"
#include "ProductionObserver.h"
#include <memory>
#include <string>
#include <cmath>

// додатковий структурний патерн GoF: Facade (Фасад)
// Об'єднує роботу з нашою БД (Repository), State, Strategy та Observer у простий інтерфейс
class ProductionLineFacade {
private:
    SQLiteEquipmentRepository& repo;
    DeviationEvaluator evaluator;
    ProductionEventManager eventManager;

public:
    ProductionLineFacade(SQLiteEquipmentRepository& repository,
        std::unique_ptr<IDeviationStrategy> initialStrategy)
        : repo(repository), evaluator(std::move(initialStrategy)) {
    }

    // Керування підписниками (Observer)
    void subscribe(const std::shared_ptr<IProductionObserver>& observer) {
        eventManager.subscribe(observer);
    }

    void unsubscribe(const std::shared_ptr<IProductionObserver>& observer) {
        eventManager.unsubscribe(observer);
    }

    // Заміна алгоритму розрахунку відхилення (Strategy)
    void setDeviationStrategy(std::unique_ptr<IDeviationStrategy> newStrategy) {
        evaluator.setStrategy(std::move(newStrategy));
    }

    std::string getCurrentStrategyName() const {
        return evaluator.getStrategyName();
    }

    // Крок 1 життєвого циклу: виготовлення деталі, запис у БД та сповіщення підписників
    std::unique_ptr<ProductContext> producePart(int equipmentId,
        const std::string& name,
        double actualValue,
        double standardValue,
        double tolerance) {
        Product prod{ -1, equipmentId, name, "Manufactured", actualValue, standardValue, tolerance };
        prod.id = repo.addProduct(prod);

        auto context = std::make_unique<ProductContext>(
            prod, repo, std::make_unique<ManufacturedState>()
        );

        ProductionEvent event{
            ProductionEventType::PartManufactured,
            prod.id,
            prod.name,
            context->getStateName(),
            0.0,
            ""
        };
        eventManager.notify(event);

        return context;
    }

    // Крок 2 життєвого циклу: перевірка якості (State + Strategy + Observer + SQLite транзакція)
    double inspectAndEvaluate(ProductContext& context) {
        // Перехід: Виготовлена -> Перевірена (з транзакцією в БД)
        if (!context.inspect()) {
            return -1.0;
        }

        const Product& p = context.getProduct();
        double deviation = evaluator.evaluate(p.actualValue, p.standardValue, p.tolerance);

        // Якщо фактичне відхилення в межах допуску -> Прийнята, інакше -> Бракована
        bool withinTolerance = std::fabs(p.actualValue - p.standardValue) <= p.tolerance;
        if (withinTolerance) {
            context.accept();
        }
        else {
            context.markDefective();
        }

        ProductionEvent event{
            ProductionEventType::QualityControlCompleted,
            p.id,
            p.name,
            context.getStateName(),
            deviation,
            evaluator.getStrategyName()
        };
        eventManager.notify(event);

        return deviation;
    }
};